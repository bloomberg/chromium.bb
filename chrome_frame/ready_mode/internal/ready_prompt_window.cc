// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/ready_mode/internal/ready_prompt_window.h"

#include "base/compiler_specific.h"
#include "chrome_frame/ready_mode/internal/ready_mode_state.h"

ReadyPromptWindow::ReadyPromptWindow()
    : frame_(NULL),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

base::WeakPtr<ReadyPromptWindow> ReadyPromptWindow::Initialize(
    InfobarContent::Frame* frame, ReadyModeState* ready_mode_state) {
  DCHECK(frame != NULL);
  DCHECK(frame_ == NULL);
  DCHECK(ready_mode_state != NULL);
  DCHECK(ready_mode_state_ == NULL);

  frame_ = frame;
  ready_mode_state_.reset(ready_mode_state);

  DCHECK(!IsWindow());

  if (Create(frame->GetFrameWindow()) == NULL) {
    DPLOG(ERROR) << "Failed to create HWND for ReadyPromptWindow.";
    delete this;
    return base::WeakPtr<ReadyPromptWindow>();
  }

  return weak_ptr_factory_.GetWeakPtr();
}

void ReadyPromptWindow::OnDestroy() {
  frame_ = NULL;
}

BOOL ReadyPromptWindow::OnInitDialog(CWindow wndFocus, LPARAM lInitParam) {
  DlgResize_Init(false);  // false => 'no gripper'
  return TRUE;
}

LRESULT ReadyPromptWindow::OnYes(WORD /*wNotifyCode*/,
                                 WORD /*wID*/,
                                 HWND /*hWndCtl*/,
                                 BOOL& /*bHandled*/) {
  frame_->CloseInfobar();
  ready_mode_state_->AcceptChromeFrame();
  return 0;
}

LRESULT ReadyPromptWindow::OnRemindMeLater(WORD /*wNotifyCode*/,
                                           WORD /*wID*/,
                                           HWND /*hWndCtl*/,
                                           BOOL& /*bHandled*/) {
  frame_->CloseInfobar();
  ready_mode_state_->TemporarilyDeclineChromeFrame();
  return 0;
}

LRESULT ReadyPromptWindow::OnNo(WORD /*wNotifyCode*/,
                                WORD /*wID*/,
                                HWND /*hWndCtl*/,
                                BOOL& /*bHandled*/) {
  frame_->CloseInfobar();
  ready_mode_state_->PermanentlyDeclineChromeFrame();
  return 0;
}

void ReadyPromptWindow::OnFinalMessage(HWND) {
  delete this;
}
