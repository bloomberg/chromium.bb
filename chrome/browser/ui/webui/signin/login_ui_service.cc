// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/common/url_constants.h"

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
  if (current_login_ui()) {
    current_login_ui()->FocusUI();
    return;
  }

  Browser* browser =
      new Browser(Browser::CreateParams(Browser::TYPE_POPUP, profile_,
                                        chrome::GetActiveDesktop()));
  // TODO(munjal): Change the source from SOURCE_NTP_LINK to something else
  // once we have added a new source for extension API.
  GURL signin_url(SyncPromoUI::GetSyncPromoURL(GURL(),
                                               SyncPromoUI::SOURCE_NTP_LINK,
                                               true));
  chrome::NavigateParams params(browser,
                                signin_url,
                                content::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = CURRENT_TAB;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  chrome::Navigate(&params);
}
