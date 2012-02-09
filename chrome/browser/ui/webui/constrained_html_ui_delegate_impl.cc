// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_html_ui_delegate_impl.h"

#include <string>

#include "base/property_bag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/constrained_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/html_dialog_tab_contents_delegate.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

ConstrainedHtmlUIDelegateImpl::ConstrainedHtmlUIDelegateImpl(
    Profile* profile,
    HtmlDialogUIDelegate* delegate,
    HtmlDialogTabContentsDelegate* tab_delegate)
    : HtmlDialogTabContentsDelegate(profile),
      html_delegate_(delegate),
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
  // Set |this| as a property so the ConstrainedHtmlUI can retrieve it.
  ConstrainedHtmlUI::GetPropertyAccessor().SetProperty(
      web_contents->GetPropertyBag(), this);

  web_contents->GetController().LoadURL(delegate->GetDialogContentURL(),
                                        content::Referrer(),
                                        content::PAGE_TRANSITION_START_PAGE,
                                        std::string());
}

ConstrainedHtmlUIDelegateImpl::~ConstrainedHtmlUIDelegateImpl() {
  if (release_tab_on_close_)
    ignore_result(tab_.release());
}

const HtmlDialogUIDelegate*
ConstrainedHtmlUIDelegateImpl::GetHtmlDialogUIDelegate() const {
  return html_delegate_;
}

HtmlDialogUIDelegate*
ConstrainedHtmlUIDelegateImpl::GetHtmlDialogUIDelegate() {
  return html_delegate_;
}

void ConstrainedHtmlUIDelegateImpl::OnDialogCloseFromWebUI() {
  closed_via_webui_ = true;
  window_->CloseConstrainedWindow();
}

void ConstrainedHtmlUIDelegateImpl::set_window(ConstrainedWindow* window) {
  window_ = window;
}

void ConstrainedHtmlUIDelegateImpl::set_override_tab_delegate(
    HtmlDialogTabContentsDelegate* override_tab_delegate) {
  override_tab_delegate_.reset(override_tab_delegate);
}

bool ConstrainedHtmlUIDelegateImpl::closed_via_webui() const {
  return closed_via_webui_;
}

void ConstrainedHtmlUIDelegateImpl::ReleaseTabContentsOnDialogClose() {
  release_tab_on_close_ = true;
}

ConstrainedWindow* ConstrainedHtmlUIDelegateImpl::window() {
  return window_;
}

TabContentsWrapper* ConstrainedHtmlUIDelegateImpl::tab() {
  return tab_.get();
}

void ConstrainedHtmlUIDelegateImpl::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
}
