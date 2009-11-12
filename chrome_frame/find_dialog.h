// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_FIND_DIALOG_H_
#define CHROME_FRAME_FIND_DIALOG_H_

#include <atlbase.h>
#include <atlwin.h>

#include "base/ref_counted.h"
#include "resource.h"
#include "grit/chrome_frame_resources.h"

class ChromeFrameAutomationClient;

class CFFindDialog : public CDialogImpl<CFFindDialog> {
 public:
  enum { IDD = IDD_FIND_DIALOG };

  BEGIN_MSG_MAP(CFFindDialog)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    COMMAND_ID_HANDLER(IDOK, OnFind)
    COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
  END_MSG_MAP()

  CFFindDialog();
  void Init(ChromeFrameAutomationClient* automation_client);

  LRESULT OnDestroy(UINT msg, WPARAM wparam,
                    LPARAM lparam, BOOL& handled);  // NOLINT
  LRESULT OnFind(WORD wNotifyCode, WORD wID,
                 HWND hWndCtl, BOOL& bHandled);  // NOLINT
  LRESULT OnCancel(WORD wNotifyCode, WORD wID,
                   HWND hWndCtl, BOOL& bHandled);  // NOLINT
  LRESULT OnInitDialog(UINT msg, WPARAM wparam,
                       LPARAM lparam, BOOL& handled);  // NOLINT

 private:

  // Since the message loop we expect to run in isn't going to be nicely
  // calling IsDialogMessage(), we need to hook the wnd proc and call it
  // ourselves. See http://support.microsoft.com/kb/q187988/
  bool InstallMessageHook();
  bool UninstallMessageHook();
  static LRESULT CALLBACK GetMsgProc(int code, WPARAM wparam, LPARAM lparam);
  static HHOOK msg_hook_;

  // We don't own these, and they must exist at least as long as we do.
  scoped_refptr<ChromeFrameAutomationClient> automation_client_;
};

#endif  // CHROME_FRAME_FIND_DIALOG_H_
