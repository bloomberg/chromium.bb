// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/turndown_prompt/turndown_prompt_window.h"

#include <atlctrls.h>
#include <commctrl.h>
#include <shellapi.h>

#include "base/compiler_specific.h"
#include "chrome_frame/ready_mode/internal/url_launcher.h"
#include "chrome_frame/simple_resource_loader.h"
#include "chrome_frame/utils.h"
#include "grit/chrome_frame_dialogs.h"
#include "grit/chromium_strings.h"

// atlctrlx.h requires 'min' and 'max' macros, the definition of which conflicts
// with STL headers. Hence we include them out of the order defined by style
// guidelines. As a result you may not refer to std::min or std::max in this
// file.
#include <minmax.h>  // NOLINT
#include <atlctrlx.h>  // NOLINT

// static
base::WeakPtr<TurndownPromptWindow> TurndownPromptWindow::CreateInstance(
    InfobarContent::Frame* frame,
    UrlLauncher* url_launcher,
    const base::Closure& uninstall_callback) {
  DCHECK(frame != NULL);
  DCHECK(url_launcher != NULL);

  base::WeakPtr<TurndownPromptWindow> instance(
      (new TurndownPromptWindow(frame, url_launcher, uninstall_callback))
          ->weak_ptr_factory_.GetWeakPtr());

  DCHECK(!instance->IsWindow());

  if (instance->Create(frame->GetFrameWindow()) == NULL) {
    DPLOG(ERROR) << "Failed to create HWND for TurndownPromptWindow.";
    return base::WeakPtr<TurndownPromptWindow>();
  }

  // Subclass the "Learn more." text to make it behave like a link. Clicks are
  // routed to OnLearnMore().
  CWindow rte = instance->GetDlgItem(IDC_TD_PROMPT_LINK);
  instance->link_.reset(new CHyperLink());
  instance->link_->SubclassWindow(rte);
  instance->link_->SetHyperLinkExtendedStyle(HLINK_NOTIFYBUTTON,
                                             HLINK_NOTIFYBUTTON);

  // Substitute the proper text given the current IE version.
  CWindow text = instance->GetDlgItem(IDC_TD_PROMPT_MESSAGE);
  string16 prompt_text(GetPromptText());
  if (!prompt_text.empty())
    text.SetWindowText(prompt_text.c_str());

  return instance;
}

TurndownPromptWindow::TurndownPromptWindow(
    InfobarContent::Frame* frame,
    UrlLauncher* url_launcher,
    const base::Closure& uninstall_closure)
    : frame_(frame),
      url_launcher_(url_launcher),
      uninstall_closure_(uninstall_closure),
      weak_ptr_factory_(this) {
}

TurndownPromptWindow::~TurndownPromptWindow() {}

void TurndownPromptWindow::OnFinalMessage(HWND) {
  delete this;
}

void TurndownPromptWindow::OnDestroy() {
  frame_ = NULL;
}

BOOL TurndownPromptWindow::OnInitDialog(CWindow wndFocus, LPARAM lInitParam) {
  DlgResize_Init(false);  // false => 'no gripper'
  return TRUE;
}

LRESULT TurndownPromptWindow::OnLearnMore(WORD /*wParam*/,
                                          LPNMHDR /*lParam*/,
                                          BOOL& /*bHandled*/) {
  url_launcher_->LaunchUrl(SimpleResourceLoader::Get(
      IDS_CHROME_FRAME_TURNDOWN_LEARN_MORE_URL));
  return 0;
}

LRESULT TurndownPromptWindow::OnUninstall(WORD /*wNotifyCode*/,
                                          WORD /*wID*/,
                                          HWND /*hWndCtl*/,
                                          BOOL& /*bHandled*/) {
  frame_->CloseInfobar();
  if (!uninstall_closure_.is_null())
    uninstall_closure_.Run();
  return 0;
}

// static
string16 TurndownPromptWindow::GetPromptText() {
  IEVersion ie_version = GetIEVersion();
  int message_id = IDS_CHROME_FRAME_TURNDOWN_TEXT_IE_NEWER;
  if (ie_version == IE_6 || ie_version == IE_7 || ie_version == IE_8)
    message_id = IDS_CHROME_FRAME_TURNDOWN_TEXT_IE_OLDER;
  return SimpleResourceLoader::GetInstance()->Get(message_id);
}
