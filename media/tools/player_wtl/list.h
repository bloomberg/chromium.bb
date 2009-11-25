// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef MEDIA_TOOLS_PLAYER_WTL_LIST_H_
#define MEDIA_TOOLS_PLAYER_WTL_LIST_H_

#include "media/tools/player_wtl/player_wtl.h"

// Recent Files list.
class CMruList : public CWindowImpl<CMruList, CListBox> {
 public:

  CMruList() {
    size_.cx = 400;
    size_.cy = 150;
  }

  HWND Create(HWND parent) {
    CWindowImpl<CMruList, CListBox>::Create(parent, rcDefault, NULL,
      WS_POPUP | WS_THICKFRAME | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
      WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
      WS_EX_CLIENTEDGE);
    if (IsWindow())
      SetFont(AtlGetStockFont(DEFAULT_GUI_FONT));
    return m_hWnd;
  }

  BOOL BuildList(CRecentDocumentList& mru) {
    ATLASSERT(IsWindow());

    ResetContent();

    int docs_size = mru.m_arrDocs.GetSize();
    for (int i = 0; i < docs_size; i++)
      InsertString(0, mru.m_arrDocs[i].szDocName);  // Reverse order in array.

    if (docs_size > 0) {
      SetCurSel(0);
      SetTopIndex(0);
    }

    return TRUE;
  }

  BOOL ShowList(int x, int y) {
    return SetWindowPos(NULL, x, y, size_.cx, size_.cy,
                        SWP_NOZORDER | SWP_SHOWWINDOW);
  }

  void HideList() {
    RECT rect;
    GetWindowRect(&rect);
    size_.cx = rect.right - rect.left;
    size_.cy = rect.bottom - rect.top;
    ShowWindow(SW_HIDE);
  }

  void FireCommand() {
    int selection = GetCurSel();
    if (selection != LB_ERR) {
      ::SetFocus(GetParent());  // Will hide this window.
      ::SendMessage(GetParent(), WM_COMMAND,
                    MAKEWPARAM((WORD)(ID_FILE_MRU_FIRST + selection),
                               LBN_DBLCLK), (LPARAM)m_hWnd);
    }
  }

  BEGIN_MSG_MAP(CMruList)
    MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
    MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnLButtonDblClk)
    MESSAGE_HANDLER(WM_KILLFOCUS, OnKillFocus)
    MESSAGE_HANDLER(WM_NCHITTEST, OnNcHitTest)
  END_MSG_MAP()

  LRESULT OnKeyDown(UINT /*message*/,
                    WPARAM wparam,
                    LPARAM /*lparam*/,
                    BOOL& handled) {
    if (wparam == VK_RETURN)
      FireCommand();
    else
      handled = FALSE;
    return 0;
  }

  LRESULT OnLButtonDblClk(UINT /*message*/,
                          WPARAM /*wparam*/,
                          LPARAM /*lparam*/,
                          BOOL& /*handled*/) {
    FireCommand();
    return 0;
  }

  LRESULT OnKillFocus(UINT /*message*/,
                      WPARAM /*wparam*/,
                      LPARAM /*lparam*/,
                      BOOL& /*handled*/) {
    HideList();
    return 0;
  }

  LRESULT OnNcHitTest(UINT message,
                      WPARAM wparam,
                      LPARAM lparam,
                      BOOL& /*handled*/) {
    LRESULT result = DefWindowProc(message, wparam, lparam);
    switch (result) {
      case HTLEFT:
      case HTTOP:
      case HTTOPLEFT:
      case HTTOPRIGHT:
      case HTBOTTOMLEFT:
        result = HTCLIENT;  // Don't allow resizing here.
        break;
      default:
        break;
    }
    return result;
  }

 private:

  SIZE size_;
};

#endif  // MEDIA_TOOLS_PLAYER_WTL_LIST_H_
