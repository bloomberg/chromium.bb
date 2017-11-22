// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_dice_internals_handler.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "google_apis/gaia/gaia_urls.h"

SigninDiceInternalsHandler::SigninDiceInternalsHandler(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  DCHECK(!profile_->IsOffTheRecord());
}

SigninDiceInternalsHandler::~SigninDiceInternalsHandler() {}

void SigninDiceInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "enableSync", base::Bind(&SigninDiceInternalsHandler::HandleEnableSync,
                               base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "disableSync", base::Bind(&SigninDiceInternalsHandler::HandleDisableSync,
                                base::Unretained(this)));
}

void SigninDiceInternalsHandler::HandleEnableSync(const base::ListValue* args) {
  if (SigninManagerFactory::GetForProfile(profile_)->IsAuthenticated()) {
    VLOG(1) << "[Dice] Cannot enable sync as profile is already authenticated";
    return;
  }

  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  DCHECK(browser);

  AccountTrackerService* tracker =
      AccountTrackerServiceFactory::GetForProfile(profile_);
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  std::vector<std::string> account_ids = token_service->GetAccounts();
  if (account_ids.empty()) {
    browser->window()->ShowAvatarBubbleFromAvatarButton(
        BrowserWindow::AVATAR_BUBBLE_MODE_SIGNIN,
        signin::ManageAccountsParams(),
        signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN, false);
    return;
  }

  std::string account_id = account_ids[0];
  std::string email = tracker->GetAccountInfo(account_id).email;
  VLOG(1) << "[Dice] Start syncing with account " << email;

  // OneClickSigninSyncStarter is suicidal (it will kill itself once it finishes
  // enabling sync).
  OneClickSigninSyncStarter::Callback callback;
  new OneClickSigninSyncStarter(
      profile_, browser, account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      signin_metrics::Reason::REASON_UNKNOWN_REASON,
      OneClickSigninSyncStarter::CURRENT_PROFILE, callback);
}

void SigninDiceInternalsHandler::HandleDisableSync(
    const base::ListValue* args) {
  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(profile_);
  if (!signin_manager->IsAuthenticated()) {
    VLOG(2) << "[Dice] Cannot disable sync as profile is not authenticated";
    return;
  }

  VLOG(2) << "[Dice] Sign out.";
  signin_manager->SignOut(signin_metrics::USER_TUNED_OFF_SYNC_FROM_DICE_UI,
                          signin_metrics::SignoutDelete::IGNORE_METRIC);
}
