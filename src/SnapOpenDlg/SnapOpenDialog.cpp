/*
This file is part of Explorer Plugin for Notepad++
Copyright (C)2006 Jens Lorenz <jens.plugin.npp@gmx.de>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include "SnapOpenDialog.h"
#include "Explorer.h"
#include "PatternMatch.h"
#include <ExplorerDialog.h>
#include <Commctrl.h>
#include <shlobj.h>


	void SnapOpenDlg::init(HINSTANCE hInst, NppData nppData)
	{
		_nppData = nppData;
		Window::init(hInst, nppData._nppHandle);
		_mustRefresh = true;
	};

// Set a call back with the handle after init to set the path.
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/shell/reference/callbackfunctions/browsecallbackproc.asp
static int __stdcall BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM, LPARAM pData)
{
	if (uMsg == BFFM_INITIALIZED)
		::SendMessage(hwnd, BFFM_SETSELECTION, TRUE, pData);
	return 0;
};


UINT SnapOpenDlg::doDialog()
{
	return ::DialogBoxParam(_hInst, MAKEINTRESOURCE(IDD_QUICK_OPEN_DLG), _hParent,  (DLGPROC)dlgProc, (LPARAM)this);
}


BOOL CALLBACK SnapOpenDlg::run_dlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message) 
	{
		case WM_INITDIALOG:
		{
			string rootPath = getRoot();
			rootPathLen = rootPath.size();
			
			goToCenter();
			if (_mustRefresh) findFiles();
			_curSelText = getSelectedText();
					//::MessageBox(_hParent, _T("Goo: "), _T("Error"), MB_OK);
		
			if (!_curSelText.empty()) {
				::MessageBox(_hParent, (_T("Selection: ") + _curSelText).c_str(), _T("Error"), MB_OK);
				Edit_SetText(GetDlgItem(_hSelf,	IDC_EDIT_SEARCH), _curSelText.c_str());			
			}

			populateResultList();
			::PostMessage(_hSelf, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(_hSelf, IDC_EDIT_SEARCH), TRUE);
 
			// Subclass the edit control. 
			SetWindowLong(GetDlgItem(_hSelf, IDC_EDIT_SEARCH), GWL_USERDATA, (LONG) this);
			wpOrigEditProc = (WNDPROC) SetWindowLong(GetDlgItem(_hSelf, IDC_EDIT_SEARCH), GWL_WNDPROC, (LONG) editSubclassProc);

			string title = _T("Snap Open - ") + rootPath;
			SetWindowText(_hSelf, title.c_str());

			break;
		}
		case WM_DESTROY: 
			// Remove the subclass from the edit control. 
			SetWindowLong(GetDlgItem(_hSelf, IDC_EDIT_SEARCH), GWL_WNDPROC, (LONG) wpOrigEditProc);
			break;
		case WM_COMMAND : 
		{
			switch (LOWORD(wParam))
			{
				case IDC_EDIT_SEARCH:
					if (HIWORD(wParam) == EN_CHANGE)
					{
						populateResultList();
					}
					break;
				case IDCANCEL:
					::EndDialog(_hSelf, IDCANCEL);
					return TRUE;
				case IDC_LIST_RESULTS:
					if (HIWORD(wParam) != LBN_DBLCLK)
					{
						break;
					}
				case IDOK:
				{
					int selection;
					string fullPath = getRoot();
					TCHAR pszFilePath[MAX_PATH];
					pszFilePath[0] = 0;
					selection = ::SendDlgItemMessage(_hSelf, IDC_LIST_RESULTS, LB_GETCURSEL, 0, 0);
					if (selection != LB_ERR)
					{
						::SendDlgItemMessage(_hSelf, IDC_LIST_RESULTS, LB_GETTEXT, selection, (LPARAM)&pszFilePath);
					}
					::EndDialog(_hSelf, IDOK);
					if (pszFilePath[0])
					{
						fullPath += pszFilePath;
						::SendMessage(_nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)fullPath.c_str());
					}
					return TRUE;
				}
				default:
					return FALSE;
			}
			break;
		}
		default:
			break;
	}
	return FALSE;
}

string SnapOpenDlg::getRoot()
{
	extern ExplorerDialog explorerDlg;

	if (rootPath.empty())
	{
		rootPath = explorerDlg.GetSelectedPath();
	}
	return rootPath;
}

void SnapOpenDlg::findFiles()
{
	::MessageBox(_hParent, (_T("refreshing folder: ") + getRoot()).c_str(), _T("Error"), MB_OK);
	files.clear();
	findFilesRecursively(getRoot().c_str());
	_mustRefresh = false;
}

void SnapOpenDlg::findFilesRecursively(LPCTSTR lpFolder)
{
	TCHAR szFullPattern[MAX_PATH];
	WIN32_FIND_DATA FindFileData;
	HANDLE hFindFile;
	// first we are going to process any subdirectories
	PathCombine(szFullPattern, lpFolder, _T("*"));
	hFindFile = FindFirstFile(szFullPattern, &FindFileData);
	if(hFindFile != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)){
				if(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					if (_tcscmp(FindFileData.cFileName, _T(".")) && _tcscmp(FindFileData.cFileName, _T("..")))
					{
						// found a subdirectory; recurse into it
						PathCombine(szFullPattern, lpFolder, FindFileData.cFileName);
						findFilesRecursively(szFullPattern);
					}
				}
				else
				{
					PathCombine(szFullPattern, lpFolder, FindFileData.cFileName);
					files.push_back(&szFullPattern[rootPathLen]);
				}
			}
		} while(FindNextFile(hFindFile, &FindFileData));
		FindClose(hFindFile);
	}
}

void SnapOpenDlg::populateResultList()
{
	TCHAR pattern[MAX_PATH];
	Edit_GetText(GetDlgItem(_hSelf, IDC_EDIT_SEARCH), pattern, _countof(pattern));
	::SendDlgItemMessage(_hSelf, IDC_LIST_RESULTS, LB_RESETCONTENT, 0, 0);
	int count = 0;
	for (vector<wstring>::const_iterator i=files.begin();i!=files.end();++i)
	{
		if (count >= 100)
		{
			::SendDlgItemMessage(_hSelf, IDC_LIST_RESULTS, LB_ADDSTRING, 0, (LPARAM)_T("-- Too many results --"));
			break;
		}
		if (patternMatch(i->c_str(), pattern))
		{
			count++;
			::SendDlgItemMessage(_hSelf, IDC_LIST_RESULTS, LB_ADDSTRING, 0, (LPARAM)(*i).c_str());
		}
	}
	::SendDlgItemMessage(_hSelf, IDC_LIST_RESULTS, LB_SETCURSEL, 0, 0);
}

LRESULT APIENTRY SnapOpenDlg::editSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
	SnapOpenDlg *dlg = (SnapOpenDlg *)(GetWindowLong(hwnd, GWL_USERDATA));
	return dlg->run_editSubclassProc(hwnd, uMsg, wParam, lParam);
}

LRESULT APIENTRY SnapOpenDlg::run_editSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
	if (uMsg == WM_KEYDOWN)
	{
		int selection = ::SendDlgItemMessage(_hSelf, IDC_LIST_RESULTS, LB_GETCURSEL, 0, 0);
		int results = ::SendDlgItemMessage(_hSelf, IDC_LIST_RESULTS, LB_GETCOUNT, 0, 0);

		if (wParam == VK_UP && selection > 0)
		{
			::SendDlgItemMessage(_hSelf, IDC_LIST_RESULTS, LB_SETCURSEL, selection-1, 0);
			return 0;
		}
		if (wParam == VK_DOWN && selection < results - 1)
		{
			::SendDlgItemMessage(_hSelf, IDC_LIST_RESULTS, LB_SETCURSEL, selection+1, 0);
			return 0;
		}
	}
 
	return ::CallWindowProc(wpOrigEditProc, hwnd, uMsg, wParam, lParam);
}


string SnapOpenDlg::getSelectedText() {
	UINT	currentDoc;
	UINT currentEdit;
	HWND curScintHandle;
	wstring _wSelText;
	
	::SendMessage(_nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&currentEdit);
	curScintHandle = (currentEdit == 0)?_nppData._scintillaMainHandle:_nppData._scintillaSecondHandle;
	
	int sel_length = ::SendMessage(curScintHandle, SCI_GETSELTEXT, 0, 0);
	if (sel_length > 1) {
	
		char *curSel = new char[sel_length];
	
	wchar_t	tmp_txt[30];
	_stprintf(tmp_txt, _T("Sel Lenght: %d"), sel_length);

	::MessageBox(_hParent, tmp_txt, _T("Error"), MB_OK);

	::SendMessage(curScintHandle, SCI_GETSELTEXT, 0, (LPARAM)curSel);
	
	int sel_length_w = ::MultiByteToWideChar(
        CP_UTF8,                // convert from UTF-8
	     MB_ERR_INVALID_CHARS,   // error on invalid chars
		 curSel,            // source UTF-8 string
		 sel_length,                 // total length of source UTF-8 string,
		                               // in CHAR's (= bytes), including end-of-string \0

        NULL,                   // unused - no conversion done in this step
		0                       // request size of destination buffer, in WCHAR's
		);
	_stprintf(tmp_txt, _T("Sel Lenght W: %d"), sel_length_w);

	::MessageBox(_hParent, tmp_txt, _T("Error"), MB_OK);
	
	wchar_t *curSelW = new wchar_t[sel_length_w];
	
	int result = ::MultiByteToWideChar(

        CP_UTF8,                // convert from UTF-8

        MB_ERR_INVALID_CHARS,   // error on invalid chars

        curSel,            // source UTF-8 string

        sel_length,                 // total length of source UTF-8 string,

                                // in CHAR's (= bytes), including end-of-string \0

        curSelW,               // destination buffer

        sel_length_w                // size of destination buffer, in WCHAR's

        );
		_wSelText = curSelW;
		delete[] curSel;
		delete[] curSelW;
	}

	return _wSelText;
}

string SnapOpenDlg::GetRootPath()
{
	return rootPath;
}

void SnapOpenDlg::SetRootPath(const string rootPath)
{
	this->rootPath = rootPath;
	_mustRefresh = true;
	::MessageBox(_hParent, (_T("Root Path Set To: ") + rootPath).c_str(), _T("Error"), MB_OK);

}