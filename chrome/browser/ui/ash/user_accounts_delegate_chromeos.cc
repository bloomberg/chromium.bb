// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/user_accounts_delegate_chromeos.h"

#include <algorithm>
#include <iterator>

#include "base/logging.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/ui/inline_login_dialog.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/mutable_profile_oauth2_token_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/user_manager/user.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace chromeos {

UserAccountsDelegateChromeOS::UserAccountsDelegateChromeOS(
    Profile* user_profile)
    : user_profile_(user_profile) {
  ProfileOAuth2TokenServiceFactory::GetForProfile(user_profile_)
      ->AddObserver(this);
}

UserAccountsDelegateChromeOS::~UserAccountsDelegateChromeOS() {
  ProfileOAuth2TokenServiceFactory::GetForProfile(user_profile_)
      ->RemoveObserver(this);
}

std::string UserAccountsDelegateChromeOS::GetPrimaryAccountId() {
  return SigninManagerFactory::GetForProfile(user_profile_)
      ->GetAuthenticatedAccountId();
}

std::vector<std::string>
UserAccountsDelegateChromeOS::GetSecondaryAccountIds() {
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(user_profile_);
  std::vector<std::string> accounts = token_service->GetAccounts();
  // Filter primary account.
  std::vector<std::string>::iterator it =
      std::remove(accounts.begin(), accounts.end(), GetPrimaryAccountId());
  LOG_IF(WARNING, std::distance(it, accounts.end()) != 1)
      << "Found " << std::distance(it, accounts.end())
      << " primary accounts in the account list.";
  accounts.erase(it, accounts.end());
  return accounts;
}

std::string UserAccountsDelegateChromeOS::GetAccountDisplayName(
    const std::string& account_id) {
  user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(user_profile_);
  if (gaia::AreEmailsSame(user->email(), account_id) &&
      !user->display_email().empty())
    return user->display_email();
  return account_id;
}

void UserAccountsDelegateChromeOS::DeleteAccount(
    const std::string& account_id) {
  MutableProfileOAuth2TokenService* oauth2_token_service =
      ProfileOAuth2TokenServiceFactory::GetPlatformSpecificForProfile(
          user_profile_);
  oauth2_token_service->RevokeCredentials(account_id);
}

void UserAccountsDelegateChromeOS::LaunchAddAccountDialog() {
  ui::InlineLoginDialog::Show(user_profile_);
}

void UserAccountsDelegateChromeOS::OnRefreshTokenAvailable(
    const std::string& account_id) {
  NotifyAccountListChanged();
}

void UserAccountsDelegateChromeOS::OnRefreshTokenRevoked(
    const std::string& account_id) {
  NotifyAccountListChanged();
}

}  // namespace chromeos
