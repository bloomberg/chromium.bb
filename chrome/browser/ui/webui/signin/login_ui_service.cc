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
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/common/profile_management_switches.h"

LoginUIService::LoginUIService(Profile* profile)
    : ui_(NULL), profile_(profile) {
}

LoginUIService::~LoginUIService() {}

void LoginUIService::AddObserver(LoginUIService::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void LoginUIService::RemoveObserver(LoginUIService::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void LoginUIService::SetLoginUI(LoginUI* ui) {
  DCHECK(!current_login_ui() || current_login_ui() == ui);
  ui_ = ui;
  FOR_EACH_OBSERVER(Observer, observer_list_, OnLoginUIShown(ui_));
}

void LoginUIService::LoginUIClosed(LoginUI* ui) {
  if (current_login_ui() != ui)
    return;

  ui_ = NULL;
  FOR_EACH_OBSERVER(Observer, observer_list_, OnLoginUIClosed(ui));
}

void LoginUIService::SyncConfirmationUIClosed(
    SyncConfirmationUIClosedResults results) {
  FOR_EACH_OBSERVER(
      Observer,
      observer_list_,
      OnSyncConfirmationUIClosed(results));
}

void LoginUIService::UntrustedLoginUIShown() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnUntrustedLoginUIShown());
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
                                        const base::string16& message) {
#if defined(OS_CHROMEOS)
  // ChromeOS doesn't have the avatar bubble so it never calls this function.
  NOTREACHED();
#endif
  last_login_result_ = message;
  browser->window()->ShowAvatarBubbleFromAvatarButton(
      message.empty() ? BrowserWindow::AVATAR_BUBBLE_MODE_CONFIRM_SIGNIN
                      : BrowserWindow::AVATAR_BUBBLE_MODE_SHOW_ERROR,
      signin::ManageAccountsParams(),
      signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS);
}

const base::string16& LoginUIService::GetLastLoginResult() {
  return last_login_result_;
}
