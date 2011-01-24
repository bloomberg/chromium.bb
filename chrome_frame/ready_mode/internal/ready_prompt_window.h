// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_READY_MODE_INTERNAL_READY_PROMPT_WINDOW_H_
#define CHROME_FRAME_READY_MODE_INTERNAL_READY_PROMPT_WINDOW_H_
#pragma once

#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlframe.h>
#include <atlwin.h>

#include "base/weak_ptr.h"
#include "base/scoped_ptr.h"
#include "base/win/scoped_comptr.h"
#include "chrome_frame/infobars/infobar_content.h"
#include "chrome_frame/resource.h"
#include "grit/generated_resources.h"

class ReadyModeState;
class UrlLauncher;

namespace WTL {
class CHyperLink;
}  // namespace WTL

// Implements a dialog with text and buttons inviting the user to permanently
// activate the product or temporarily/permanently disable Ready Mode.
class ReadyPromptWindow
    : public CDialogImpl<ReadyPromptWindow, CWindow>,
      public CDialogResize<ReadyPromptWindow> {
 public:
  enum { IDD = IDD_CHROME_FRAME_READY_PROMPT };

  // Creates and initializes a dialog for display in the provided frame. The
  // ReadyModeState will be invoked to capture the user's response, if any.
  // The UrlLauncher may be used to launch a new tab containing a
  // knowledge-base article about Ready Mode.
  //
  // Upon success, takes ownership of itself (to be deleted upon WM_DESTROY) and
  // returns a weak pointer to this dialog. Upon failure, returns a null weak
  // pointer and deletes self.
  //
  // In either case, takes ownership of the ReadyModeState and UrlLauncher, but
  // not the frame.
  static base::WeakPtr<ReadyPromptWindow> CreateInstance(
      InfobarContent::Frame* frame,
      ReadyModeState* ready_mode_state,
      UrlLauncher* url_launcher);

  BEGIN_MSG_MAP(InfobarWindow)
    MSG_WM_INITDIALOG(OnInitDialog)
    MSG_WM_DESTROY(OnDestroy)
    NOTIFY_HANDLER(IDC_PROMPT_LINK, NM_CLICK, OnLearnMore)
    COMMAND_HANDLER(IDACTIVATE, BN_CLICKED, OnYes)
    COMMAND_HANDLER(IDNEVER, BN_CLICKED, OnNo)
    CHAIN_MSG_MAP(CDialogResize<ReadyPromptWindow>)
  END_MSG_MAP()

  BEGIN_DLGRESIZE_MAP(InfobarWindow)
    DLGRESIZE_CONTROL(IDACTIVATE, DLSZ_CENTER_Y | DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDNEVER, DLSZ_CENTER_Y | DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_PROMPT_MESSAGE, DLSZ_SIZE_Y | DLSZ_SIZE_X)
    DLGRESIZE_CONTROL(IDC_PROMPT_LINK, DLSZ_CENTER_Y | DLSZ_MOVE_X)
  END_DLGRESIZE_MAP()

  virtual void OnFinalMessage(HWND);

 private:
  // Call CreateInstance() to get an initialized ReadyPromptWindow.
  ReadyPromptWindow(InfobarContent::Frame* frame,
                    ReadyModeState* ready_mode_state,
                    UrlLauncher* url_launcher);

  // The ReadyPromptWindow manages its own destruction.
  virtual ~ReadyPromptWindow();

  // Event handlers.
  void OnDestroy();
  BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam);
  LRESULT OnYes(WORD wNotifyCode,
                WORD wID,
                HWND hWndCtl,
                BOOL& bHandled);
  LRESULT OnRemindMeLater(WORD wNotifyCode,
                          WORD wID,
                          HWND hWndCtl,
                          BOOL& bHandled);
  LRESULT OnLearnMore(WORD wParam, LPNMHDR lParam, BOOL& bHandled);  // NOLINT
  LRESULT OnNo(WORD wNotifyCode,
               WORD wID,
               HWND hWndCtl,
               BOOL& bHandled);

  InfobarContent::Frame* frame_;  // Not owned by this instance
  scoped_ptr<ReadyModeState> ready_mode_state_;
  scoped_ptr<WTL::CHyperLink> link_;
  scoped_ptr<UrlLauncher> url_launcher_;

  base::WeakPtrFactory<ReadyPromptWindow> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ReadyPromptWindow);
};  // class ReadyPromptWindow

#endif  // CHROME_FRAME_READY_MODE_INTERNAL_READY_PROMPT_WINDOW_H_
