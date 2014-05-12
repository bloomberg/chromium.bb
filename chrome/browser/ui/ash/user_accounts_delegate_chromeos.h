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
  virtual ~UserAccountsDelegateChromeOS();

  // Overridden from ash::tray::UserAccountsDelegate:
  virtual std::string GetPrimaryAccountId() OVERRIDE;
  virtual std::vector<std::string> GetSecondaryAccountIds() OVERRIDE;
  virtual std::string GetAccountDisplayName(
      const std::string& account_id) OVERRIDE;
  virtual void DeleteAccount(const std::string& account_id) OVERRIDE;
  virtual void LaunchAddAccountDialog() OVERRIDE;

  // Overridden from OAuth2TokenServiceObserver:
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE;
  virtual void OnRefreshTokenRevoked(const std::string& account_id) OVERRIDE;

 private:
  Profile* user_profile_;

  DISALLOW_COPY_AND_ASSIGN(UserAccountsDelegateChromeOS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_ASH_USER_ACCOUNTS_DELEGATE_CHROMEOS_H_
