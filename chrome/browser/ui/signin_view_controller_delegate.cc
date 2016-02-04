// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin_view_controller_delegate.h"

#include "chrome/browser/ui/signin_view_controller.h"
#include "chrome/browser/ui/webui/signin/get_auth_frame.h"
#include "content/public/browser/web_contents.h"

namespace {

content::WebContents* GetAuthFrameWebContents(
    content::WebContents* web_ui_web_contents) {
  return signin::GetAuthFrameWebContents(web_ui_web_contents, "signin-frame");
}

}  // namespace

SigninViewControllerDelegate::SigninViewControllerDelegate(
    SigninViewController* signin_view_controller,
    content::WebContents* web_contents)
    : signin_view_controller_(signin_view_controller) {
  web_contents->SetDelegate(this);
}

SigninViewControllerDelegate::~SigninViewControllerDelegate() {}

void SigninViewControllerDelegate::CloseModalSignin() {
  ResetSigninViewControllerDelegate();
  PerformClose();
}

void SigninViewControllerDelegate::ResetSigninViewControllerDelegate() {
  if (signin_view_controller_) {
    signin_view_controller_->ResetModalSigninDelegate();
    signin_view_controller_ = nullptr;
  }
}

void SigninViewControllerDelegate::NavigationButtonClicked(
    content::WebContents* web_contents) {
  if (CanGoBack(web_contents)) {
    auto auth_web_contents = GetAuthFrameWebContents(web_contents);
    auth_web_contents->GetController().GoBack();
  } else {
    CloseModalSignin();
  }
}

bool SigninViewControllerDelegate::CanGoBack(
    content::WebContents* web_ui_web_contents) const {
  auto auth_web_contents = GetAuthFrameWebContents(web_ui_web_contents);
  return auth_web_contents && auth_web_contents->GetController().CanGoBack();
}

// content::WebContentsDelegate
void SigninViewControllerDelegate::LoadingStateChanged(
    content::WebContents* source,
    bool to_different_document) {
  if (CanGoBack(source))
    ShowBackArrow();
  else
    ShowCloseButton();
}
