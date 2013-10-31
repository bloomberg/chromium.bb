// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/sync/inline_login_dialog.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/app_mode/app_mode_utils.h"
#endif

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

void LoginUIService::ShowLoginPopup() {
#if defined(OS_CHROMEOS)
  if (chrome::IsRunningInForcedAppMode())
    InlineLoginDialog::Show(profile_);
#else
  chrome::ScopedTabbedBrowserDisplayer displayer(
      profile_, chrome::GetActiveDesktop());
  chrome::ShowBrowserSignin(displayer.browser(), signin::SOURCE_APP_LAUNCHER);
#endif
}
