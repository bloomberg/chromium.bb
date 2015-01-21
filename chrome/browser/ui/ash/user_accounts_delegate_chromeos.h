// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_USER_ACCOUNTS_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_USER_ACCOUNTS_DELEGATE_CHROMEOS_H_

#include "ash/system/user/user_accounts_delegate.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "google_apis/gaia/oauth2_token_service.h"

class Profile;

namespace chromeos {

class UserAccountsDelegateChromeOS : public ash::tray::UserAccountsDelegate,
                                     public OAuth2TokenService::Observer {
 public:
  explicit UserAccountsDelegateChromeOS(Profile* user_profile);
  ~UserAccountsDelegateChromeOS() override;

  // Overridden from ash::tray::UserAccountsDelegate:
  std::string GetPrimaryAccountId() override;
  std::vector<std::string> GetSecondaryAccountIds() override;
  std::string GetAccountDisplayName(const std::string& account_id) override;
  void DeleteAccount(const std::string& account_id) override;
  void LaunchAddAccountDialog() override;

  // Overridden from OAuth2TokenServiceObserver:
  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokenRevoked(const std::string& account_id) override;

 private:
  Profile* user_profile_;

  DISALLOW_COPY_AND_ASSIGN(UserAccountsDelegateChromeOS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_ASH_USER_ACCOUNTS_DELEGATE_CHROMEOS_H_
