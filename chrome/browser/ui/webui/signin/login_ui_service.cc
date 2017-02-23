// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_service.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/common/profile_management_switches.h"

LoginUIService::LoginUIService(Profile* profile)
#if !defined(OS_CHROMEOS)
    : profile_(profile)
#endif
{
}

LoginUIService::~LoginUIService() {}

void LoginUIService::AddObserver(LoginUIService::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void LoginUIService::RemoveObserver(LoginUIService::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

LoginUIService::LoginUI* LoginUIService::current_login_ui() const {
  return ui_list_.empty() ? nullptr : ui_list_.front();
}

void LoginUIService::SetLoginUI(LoginUI* ui) {
  ui_list_.remove(ui);
  ui_list_.push_front(ui);
}

void LoginUIService::LoginUIClosed(LoginUI* ui) {
  ui_list_.remove(ui);
}

void LoginUIService::SyncConfirmationUIClosed(
    SyncConfirmationUIClosedResult result) {
  for (Observer& observer : observer_list_)
    observer.OnSyncConfirmationUIClosed(result);
}

void LoginUIService::ShowLoginPopup() {
#if defined(OS_CHROMEOS)
  NOTREACHED();
#else
  chrome::ScopedTabbedBrowserDisplayer displayer(profile_);
  chrome::ShowBrowserSignin(
      displayer.browser(),
      signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS);
#endif
}

void LoginUIService::DisplayLoginResult(Browser* browser,
                                        const base::string16& error_message,
                                        const base::string16& email) {
#if defined(OS_CHROMEOS)
  // ChromeOS doesn't have the avatar bubble so it never calls this function.
  NOTREACHED();
#endif
  last_login_result_ = error_message;
  last_login_error_email_ = email;
  if (switches::IsMaterialDesignUserMenu() && !error_message.empty()) {
    if (browser)
      browser->ShowModalSigninErrorWindow();
    else
      UserManagerProfileDialog::DisplayErrorMessage();
  } else if (browser) {
    browser->window()->ShowAvatarBubbleFromAvatarButton(
        error_message.empty() ? BrowserWindow::AVATAR_BUBBLE_MODE_CONFIRM_SIGNIN
                              : BrowserWindow::AVATAR_BUBBLE_MODE_SHOW_ERROR,
        signin::ManageAccountsParams(),
        signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS, false);
  }
}

const base::string16& LoginUIService::GetLastLoginResult() const {
  return last_login_result_;
}

const base::string16& LoginUIService::GetLastLoginErrorEmail() const {
  return last_login_error_email_;
}
