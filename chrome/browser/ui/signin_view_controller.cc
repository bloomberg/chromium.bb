// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin_view_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "google_apis/gaia/gaia_urls.h"

SigninViewController::SigninViewController() : delegate_(nullptr) {}

SigninViewController::~SigninViewController() {
  CloseModalSignin();
}

// static
bool SigninViewController::ShouldShowSigninForMode(
    profiles::BubbleViewMode mode) {
  return mode == profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN ||
         mode == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
         mode == profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH;
}

void SigninViewController::ShowSignin(
    profiles::BubbleViewMode mode,
    Browser* browser,
    signin_metrics::AccessPoint access_point) {
  DCHECK(ShouldShowSigninForMode(mode));
  if (signin::IsDicePrepareMigrationEnabled()) {
    ShowDiceSigninTab(browser);
  } else {
    ShowModalSigninDialog(mode, browser, access_point);
  }
}

void SigninViewController::ShowModalSigninDialog(
    profiles::BubbleViewMode mode,
    Browser* browser,
    signin_metrics::AccessPoint access_point) {
  CloseModalSignin();
  // The delegate will delete itself on request of the UI code when the widget
  // is closed.
  delegate_ = SigninViewControllerDelegate::CreateModalSigninDelegate(
      this, mode, browser, access_point);

  // When the user has a proxy that requires HTTP auth, loading the sign-in
  // dialog can trigger the HTTP auth dialog.  This means the signin view
  // controller needs a dialog manager to handle any such dialog.
  delegate_->AttachDialogManager();
  chrome::RecordDialogCreation(chrome::DialogIdentifier::SIGN_IN);
}

void SigninViewController::ShowModalSyncConfirmationDialog(Browser* browser) {
  CloseModalSignin();
  // The delegate will delete itself on request of the UI code when the widget
  // is closed.
  delegate_ = SigninViewControllerDelegate::CreateSyncConfirmationDelegate(
      this, browser);
  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::SIGN_IN_SYNC_CONFIRMATION);
}

void SigninViewController::ShowModalSigninErrorDialog(Browser* browser) {
  CloseModalSignin();
  // The delegate will delete itself on request of the UI code when the widget
  // is closed.
  delegate_ =
      SigninViewControllerDelegate::CreateSigninErrorDelegate(this, browser);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::SIGN_IN_ERROR);
}

bool SigninViewController::ShowsModalDialog() {
  return delegate_ != nullptr;
}

void SigninViewController::CloseModalSignin() {
  if (delegate_)
    delegate_->CloseModalSignin();

  DCHECK(!delegate_);
}

void SigninViewController::SetModalSigninHeight(int height) {
  if (delegate_)
    delegate_->ResizeNativeView(height);
}

void SigninViewController::PerformNavigation() {
  if (delegate_)
    delegate_->PerformNavigation();
}

void SigninViewController::ResetModalSigninDelegate() {
  delegate_ = nullptr;
}

void SigninViewController::ShowDiceSigninTab(Browser* browser) {
  chrome::ShowSingletonTab(browser, GaiaUrls::GetInstance()->add_account_url());
  content::WebContents* active_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  DCHECK_EQ(GaiaUrls::GetInstance()->add_account_url(),
            active_contents->GetVisibleURL());
  DiceTabHelper::CreateForWebContents(active_contents);
}

content::WebContents*
SigninViewController::GetModalDialogWebContentsForTesting() {
  DCHECK(delegate_);
  return delegate_->web_contents();
}
