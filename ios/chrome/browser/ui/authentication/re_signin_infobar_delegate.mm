// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/re_signin_infobar_delegate.h"

#import <UIKit/UIKit.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

// static
bool ReSignInInfoBarDelegate::Create(ios::ChromeBrowserState* browser_state,
                                     Tab* tab) {
  infobars::InfoBarManager* infobar_manager = [tab infoBarManager];
  DCHECK(infobar_manager);

  std::unique_ptr<infobars::InfoBar> infobar =
      ReSignInInfoBarDelegate::CreateInfoBar(infobar_manager, browser_state);
  if (!infobar)
    return false;
  return !!infobar_manager->AddInfoBar(std::move(infobar));
}

// static
std::unique_ptr<infobars::InfoBar> ReSignInInfoBarDelegate::CreateInfoBar(
    infobars::InfoBarManager* infobar_manager,
    ios::ChromeBrowserState* browser_state) {
  DCHECK(infobar_manager);
  std::unique_ptr<ReSignInInfoBarDelegate> delegate =
      ReSignInInfoBarDelegate::CreateInfoBarDelegate(browser_state);
  if (!delegate)
    return nullptr;
  return infobar_manager->CreateConfirmInfoBar(std::move(delegate));
}

// static
std::unique_ptr<ReSignInInfoBarDelegate>
ReSignInInfoBarDelegate::CreateInfoBarDelegate(
    ios::ChromeBrowserState* browser_state) {
  DCHECK(browser_state);
  // Do not ask user to sign in if current profile is incognito.
  if (browser_state->IsOffTheRecord())
    return nullptr;
  // Returns null if user does not need to be prompted to sign in again.
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  if (!authService->ShouldPromptForSignIn())
    return nullptr;
  // Returns null if user has already signed in via some other path.
  if ([authService->GetAuthenticatedUserEmail() length]) {
    authService->SetPromptForSignIn(false);
    return nullptr;
  }
  base::RecordAction(
      base::UserMetricsAction("Signin_Impression_FromReSigninInfobar"));
  // User needs to be reminded to sign in again. Creates a new infobar delegate
  // and returns it.
  return base::MakeUnique<ReSignInInfoBarDelegate>(browser_state);
}

ReSignInInfoBarDelegate::ReSignInInfoBarDelegate(
    ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state),
      icon_([UIImage imageNamed:@"infobar_warning"]) {
  DCHECK(browser_state_);
  DCHECK(!browser_state_->IsOffTheRecord());
}

ReSignInInfoBarDelegate::~ReSignInInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
ReSignInInfoBarDelegate::GetIdentifier() const {
  return RE_SIGN_IN_INFOBAR_DELEGATE;
}

base::string16 ReSignInInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_IOS_SYNC_LOGIN_INFO_OUT_OF_DATE);
}

int ReSignInInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 ReSignInInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(
      IDS_IOS_SYNC_INFOBAR_SIGN_IN_SETTINGS_BUTTON_MOBILE);
}

gfx::Image ReSignInInfoBarDelegate::GetIcon() const {
  return icon_;
}

bool ReSignInInfoBarDelegate::Accept() {
  base::RecordAction(
      base::UserMetricsAction("Signin_Signin_FromReSigninInfobar"));
  UIView* infobarView = static_cast<InfoBarIOS*>(infobar())->view();
  DCHECK(infobarView);
  base::scoped_nsobject<ShowSigninCommand> command([[ShowSigninCommand alloc]
      initWithOperation:AUTHENTICATION_OPERATION_REAUTHENTICATE
      signInAccessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_RESIGNIN_INFOBAR]);
  [infobarView chromeExecuteCommand:command];

  // Stop displaying the infobar once user interacted with it.
  AuthenticationServiceFactory::GetForBrowserState(browser_state_)
      ->SetPromptForSignIn(false);
  return true;
}

void ReSignInInfoBarDelegate::InfoBarDismissed() {
  // Stop displaying the infobar once user interacted with it.
  AuthenticationServiceFactory::GetForBrowserState(browser_state_)
      ->SetPromptForSignIn(false);
}
