// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/find_dialog.h"

#include <Richedit.h>

#include "chrome_frame/chrome_frame_automation.h"

const int kMaxFindChars = 1024;

HHOOK CFFindDialog::msg_hook_ = NULL;

CFFindDialog::CFFindDialog() {}

void CFFindDialog::Init(ChromeFrameAutomationClient* automation_client) {
  automation_client_ = automation_client;
}

LRESULT CFFindDialog::OnDestroy(UINT msg, WPARAM wparam, LPARAM lparam,
                                BOOL& handled) {
  UninstallMessageHook();
  return 0;
}

LRESULT CFFindDialog::OnFind(WORD wNotifyCode, WORD wID, HWND hWndCtl,
                             BOOL& bHandled) {
  wchar_t buffer[kMaxFindChars + 1];
  GetDlgItemText(IDC_FIND_TEXT, buffer, kMaxFindChars);
  std::wstring find_text(buffer);

  bool match_case = IsDlgButtonChecked(IDC_MATCH_CASE) == BST_CHECKED;
  bool search_down = IsDlgButtonChecked(IDC_DIRECTION_DOWN) == BST_CHECKED;

  automation_client_->FindInPage(find_text,
                                 search_down ? FWD : BACK,
                                 match_case ? CASE_SENSITIVE : IGNORE_CASE,
                                 false);

  return 0;
}

LRESULT CFFindDialog::OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl,
                               BOOL& bHandled) {
  DestroyWindow();
  return 0;
}

LRESULT CFFindDialog::OnInitDialog(UINT msg, WPARAM wparam, LPARAM lparam,
                                   BOOL& handled) {
  // Init() must be called before Create() or DoModal()!
  DCHECK(automation_client_.get());

  InstallMessageHook();
  SendDlgItemMessage(IDC_FIND_TEXT, EM_EXLIMITTEXT, 0, kMaxFindChars);
  BOOL result = CheckRadioButton(IDC_DIRECTION_DOWN, IDC_DIRECTION_UP,
                                 IDC_DIRECTION_DOWN);

  HWND text_field = GetDlgItem(IDC_FIND_TEXT);
  ::SetFocus(text_field);

  return FALSE;  // we set the focus ourselves.
}

LRESULT CALLBACK CFFindDialog::GetMsgProc(int code, WPARAM wparam,
                                          LPARAM lparam) {
  // Mostly borrowed from http://support.microsoft.com/kb/q187988/
  // and http://www.codeproject.com/KB/atl/cdialogmessagehook.aspx.
  LPMSG msg = reinterpret_cast<LPMSG>(lparam);
  if (code >= 0 && wparam == PM_REMOVE &&
      msg->message >= WM_KEYFIRST && msg->message <= WM_KEYLAST) {
    HWND hwnd = GetActiveWindow();
    if (::IsWindow(hwnd) && ::IsDialogMessage(hwnd, msg)) {
      // The value returned from this hookproc is ignored, and it cannot
      // be used to tell Windows the message has been handled. To avoid
      // further processing, convert the message to WM_NULL before
      // returning.
      msg->hwnd = NULL;
      msg->message = WM_NULL;
      msg->lParam = 0L;
      msg->wParam = 0;
    }
  }

  // Passes the hook information to the next hook procedure in
  // the current hook chain.
  return ::CallNextHookEx(msg_hook_, code, wparam, lparam);
}

bool CFFindDialog::InstallMessageHook() {
  // Make sure we only call this once.
  DCHECK(msg_hook_ == NULL);
  msg_hook_ = ::SetWindowsHookEx(WH_GETMESSAGE, &CFFindDialog::GetMsgProc,
                                 _AtlBaseModule.m_hInst, GetCurrentThreadId());
  DCHECK(msg_hook_ != NULL);
  return msg_hook_ != NULL;
}

bool CFFindDialog::UninstallMessageHook() {
  DCHECK(msg_hook_ != NULL);
  BOOL result = ::UnhookWindowsHookEx(msg_hook_);
  DCHECK(result);
  msg_hook_ = NULL;

  return result != FALSE;
}
