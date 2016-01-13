// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_CHROME_USER_MANAGER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_CHROME_USER_MANAGER_UTIL_H_

#include "chromeos/login/login_state.h"

class AccountId;

namespace user_manager {
class User;
}

namespace chromeos {
namespace chrome_user_manager_util {

// Implements user_manager::UserManager::GetPlatformKnownUserId for ChromeOS
bool GetPlatformKnownUserId(const std::string& user_email,
                            const std::string& gaia_id,
                            AccountId* out_account_id);

// Implements user_manager::UserManager::UpdateLoginState.
void UpdateLoginState(const user_manager::User* active_user,
                      const user_manager::User* primary_user,
                      bool is_current_user_owner);

}  // namespace chrome_user_manager_util
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_CHROME_USER_MANAGER_UTIL_H_
