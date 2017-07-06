// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_dice_internals_handler.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"

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
}

void SigninDiceInternalsHandler::HandleEnableSync(const base::ListValue* args) {
  if (SigninManagerFactory::GetForProfile(profile_)->IsAuthenticated()) {
    VLOG(1) << "[Dice] Cannot enable sync as profile is already authenticated";
    return;
  }

  AccountTrackerService* tracker =
      AccountTrackerServiceFactory::GetForProfile(profile_);
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  std::vector<std::string> account_ids = token_service->GetAccounts();
  if (account_ids.empty()) {
    VLOG(1) << "[Dice] No accounts available in the token service";
    return;
  }

  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  DCHECK(browser);
  std::string account_id = account_ids[0];
  std::string gaia_id = tracker->GetAccountInfo(account_id).gaia;
  std::string email = tracker->GetAccountInfo(account_id).email;
  VLOG(1) << "[Dice] Start syncing with account " << email;

  // OneClickSigninSyncStarter is suicidal (it will kill itself once it finishes
  // enabling sync).
  OneClickSigninSyncStarter::Callback callback;
  new OneClickSigninSyncStarter(
      profile_, browser, gaia_id, email, "" /* password */,
      "" /* refresh_token */, OneClickSigninSyncStarter::CURRENT_PROFILE,
      OneClickSigninSyncStarter::CONFIRM_SYNC_SETTINGS_FIRST,
      web_ui()->GetWebContents(),
      OneClickSigninSyncStarter::CONFIRM_AFTER_SIGNIN,
      GURL("") /* current_url */, GURL("") /* continue_url */, callback);
}
