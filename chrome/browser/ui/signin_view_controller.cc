// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin_view_controller.h"

#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "components/signin/core/common/profile_management_switches.h"

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
}

void SigninViewController::ShowModalSyncConfirmationDialog(Browser* browser) {
  CloseModalSignin();
  // The delegate will delete itself on request of the UI code when the widget
  // is closed.
  signin_view_controller_delegate_ =
      SigninViewControllerDelegate::CreateSyncConfirmationDelegate(this,
                                                                   browser);
}

void SigninViewController::CloseModalSignin() {
  if (signin_view_controller_delegate_)
    signin_view_controller_delegate_->CloseModalSignin();

  DCHECK(!signin_view_controller_delegate_);
}

void SigninViewController::ResetModalSigninDelegate() {
  signin_view_controller_delegate_ = nullptr;
}

// static
bool SigninViewController::ShouldShowModalSigninForMode(
    profiles::BubbleViewMode mode) {
  return switches::UsePasswordSeparatedSigninFlow() &&
         (mode == profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN ||
          mode == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
          mode == profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH);
}
