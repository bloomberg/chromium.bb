// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_web_dialog_delegate_base.h"

#include <string>

#include "base/property_bag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/constrained_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/web_dialog_delegate.h"
#include "chrome/browser/ui/webui/web_dialog_ui.h"
#include "chrome/browser/ui/webui/web_dialog_web_contents_delegate.h"
#include "content/public/browser/web_contents.h"

using content::NativeWebKeyboardEvent;
using content::WebContents;

ConstrainedWebDialogDelegateBase::ConstrainedWebDialogDelegateBase(
    Profile* profile,
    WebDialogDelegate* delegate,
    WebDialogWebContentsDelegate* tab_delegate)
    : WebDialogWebContentsDelegate(profile),
      web_dialog_delegate_(delegate),
      window_(NULL),
      closed_via_webui_(false),
      release_tab_on_close_(false) {
  CHECK(delegate);
  WebContents* web_contents =
      WebContents::Create(profile, NULL, MSG_ROUTING_NONE, NULL, NULL);
  tab_.reset(new TabContentsWrapper(web_contents));
  if (tab_delegate) {
    override_tab_delegate_.reset(tab_delegate);
    web_contents->SetDelegate(tab_delegate);
  } else {
    web_contents->SetDelegate(this);
  }
  // Set |this| as a property so the ConstrainedWebDialogUI can retrieve it.
  ConstrainedWebDialogUI::GetPropertyAccessor().SetProperty(
      web_contents->GetPropertyBag(), this);

  web_contents->GetController().LoadURL(delegate->GetDialogContentURL(),
                                        content::Referrer(),
                                        content::PAGE_TRANSITION_START_PAGE,
                                        std::string());
}

ConstrainedWebDialogDelegateBase::~ConstrainedWebDialogDelegateBase() {
  if (release_tab_on_close_)
    ignore_result(tab_.release());
}

const WebDialogDelegate*
    ConstrainedWebDialogDelegateBase::GetWebDialogDelegate() const {
  return web_dialog_delegate_;
}

WebDialogDelegate*
    ConstrainedWebDialogDelegateBase::GetWebDialogDelegate() {
  return web_dialog_delegate_;
}

void ConstrainedWebDialogDelegateBase::OnDialogCloseFromWebUI() {
  closed_via_webui_ = true;
  window_->CloseConstrainedWindow();
}

void ConstrainedWebDialogDelegateBase::set_window(ConstrainedWindow* window) {
  window_ = window;
}

void ConstrainedWebDialogDelegateBase::set_override_tab_delegate(
    WebDialogWebContentsDelegate* override_tab_delegate) {
  override_tab_delegate_.reset(override_tab_delegate);
}

bool ConstrainedWebDialogDelegateBase::closed_via_webui() const {
  return closed_via_webui_;
}

void ConstrainedWebDialogDelegateBase::ReleaseTabContentsOnDialogClose() {
  release_tab_on_close_ = true;
}

ConstrainedWindow* ConstrainedWebDialogDelegateBase::window() {
  return window_;
}

TabContentsWrapper* ConstrainedWebDialogDelegateBase::tab() {
  return tab_.get();
}

void ConstrainedWebDialogDelegateBase::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
}
