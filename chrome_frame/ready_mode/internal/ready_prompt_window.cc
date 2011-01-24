// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/ready_mode/internal/ready_prompt_window.h"

#include <atlctrls.h>
#include <Shellapi.h>  // Must appear before atlctrlx.h

// These seem to be required by atlctrlx?
template<class A>
A min(A const& a, A const& b) { return a < b ? a : b; }

template<class A>
A max(A const& a, A const& b) { return a > b ? a : b; }

#include <atlctrlx.h>

#include "base/compiler_specific.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "chrome_frame/ready_mode/internal/ready_mode_state.h"
#include "chrome_frame/ready_mode/internal/url_launcher.h"
#include "chrome_frame/simple_resource_loader.h"
#include "grit/chromium_strings.h"

ReadyPromptWindow::ReadyPromptWindow(
    InfobarContent::Frame* frame, ReadyModeState* ready_mode_state,
    UrlLauncher* url_launcher)
    : frame_(frame),
      ready_mode_state_(ready_mode_state),
      url_launcher_(url_launcher),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}
ReadyPromptWindow::~ReadyPromptWindow() {
}

base::WeakPtr<ReadyPromptWindow> ReadyPromptWindow::CreateInstance(
    InfobarContent::Frame* frame, ReadyModeState* ready_mode_state,
    UrlLauncher* url_launcher) {
  DCHECK(frame != NULL);
  DCHECK(ready_mode_state != NULL);
  DCHECK(url_launcher != NULL);

  base::WeakPtr<ReadyPromptWindow> instance((new ReadyPromptWindow(
      frame, ready_mode_state, url_launcher))->weak_ptr_factory_.GetWeakPtr());

  DCHECK(!instance->IsWindow());

  if (instance->Create(frame->GetFrameWindow()) == NULL) {
    DPLOG(ERROR) << "Failed to create HWND for ReadyPromptWindow.";
    return base::WeakPtr<ReadyPromptWindow>();
  }

  // Subclass the "Learn more." text to make it behave like a link. Clicks are
  // routed to OnLearnMore().
  CWindow rte = instance->GetDlgItem(IDC_PROMPT_LINK);
  instance->link_.reset(new CHyperLink());
  instance->link_->SubclassWindow(rte);
  instance->link_->SetHyperLinkExtendedStyle(HLINK_NOTIFYBUTTON,
                                             HLINK_NOTIFYBUTTON);

  return instance;
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

LRESULT ReadyPromptWindow::OnLearnMore(WORD /*wParam*/,
                                       LPNMHDR /*lParam*/,
                                       BOOL& /*bHandled*/) {
  url_launcher_->LaunchUrl(SimpleResourceLoader::Get(
      IDS_CHROME_FRAME_READY_MODE_LEARN_MORE_URL));
  return 0;
}

void ReadyPromptWindow::OnFinalMessage(HWND) {
  delete this;
}
