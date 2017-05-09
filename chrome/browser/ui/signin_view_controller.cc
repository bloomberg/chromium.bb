// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin_view_controller.h"

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"

SigninViewController::SigninViewController()
    : signin_view_controller_delegate_(nullptr) {}

SigninViewController::~SigninViewController() {
  CloseModalSignin();
}

void SigninViewController::ShowModalSignin(
    profiles::BubbleViewMode mode,
    Browser* browser,
    signin_metrics::AccessPoint access_point) {
  CloseModalSignin();
  // The delegate will delete itself on request of the UI code when the widget
  // is closed.
  signin_view_controller_delegate_ =
      SigninViewControllerDelegate::CreateModalSigninDelegate(
          this, mode, browser, access_point);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::SIGN_IN);
}

void SigninViewController::ShowModalSyncConfirmationDialog(Browser* browser) {
  CloseModalSignin();
  // The delegate will delete itself on request of the UI code when the widget
  // is closed.
  signin_view_controller_delegate_ =
      SigninViewControllerDelegate::CreateSyncConfirmationDelegate(this,
                                                                   browser);
  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::SIGN_IN_SYNC_CONFIRMATION);
}

void SigninViewController::ShowModalSigninErrorDialog(Browser* browser) {
  CloseModalSignin();
  // The delegate will delete itself on request of the UI code when the widget
  // is closed.
  signin_view_controller_delegate_ =
      SigninViewControllerDelegate::CreateSigninErrorDelegate(this, browser);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::SIGN_IN_ERROR);
}

void SigninViewController::CloseModalSignin() {
  if (signin_view_controller_delegate_)
    signin_view_controller_delegate_->CloseModalSignin();

  DCHECK(!signin_view_controller_delegate_);
}

void SigninViewController::SetModalSigninHeight(int height) {
  if (signin_view_controller_delegate_)
    signin_view_controller_delegate_->ResizeNativeView(height);
}

void SigninViewController::ResetModalSigninDelegate() {
  signin_view_controller_delegate_ = nullptr;
}

// static
bool SigninViewController::ShouldShowModalSigninForMode(
    profiles::BubbleViewMode mode) {
  return mode == profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN ||
         mode == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
         mode == profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH;
}
