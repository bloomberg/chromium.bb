// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/chrome_user_manager_util.h"

#include "components/user_manager/user.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"

namespace chromeos {
namespace chrome_user_manager_util {

bool GetPlatformKnownUserId(const std::string& user_email,
                            const std::string& gaia_id,
                            AccountId* out_account_id) {
  if (user_email == user_manager::kStubUserEmail) {
    *out_account_id = user_manager::StubAccountId();
    return true;
  }

  if (user_email == user_manager::kGuestUserName) {
    *out_account_id = user_manager::GuestAccountId();
    return true;
  }
  return false;
}

void UpdateLoginState(const user_manager::User* active_user,
                      const user_manager::User* primary_user,
                      bool is_current_user_owner) {
  if (!chromeos::LoginState::IsInitialized())
    return;  // LoginState may not be initialized in tests.

  chromeos::LoginState::LoggedInState logged_in_state;
  logged_in_state = active_user ? chromeos::LoginState::LOGGED_IN_ACTIVE
                                : chromeos::LoginState::LOGGED_IN_NONE;

  chromeos::LoginState::LoggedInUserType login_user_type;
  if (logged_in_state == chromeos::LoginState::LOGGED_IN_NONE)
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_NONE;
  else if (is_current_user_owner)
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_OWNER;
  else if (active_user->GetType() == user_manager::USER_TYPE_GUEST)
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_GUEST;
  else if (active_user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT)
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT;
  else if (active_user->GetType() == user_manager::USER_TYPE_SUPERVISED)
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_SUPERVISED;
  else if (active_user->GetType() == user_manager::USER_TYPE_KIOSK_APP)
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_KIOSK_APP;
  else if (active_user->GetType() == user_manager::USER_TYPE_ARC_KIOSK_APP)
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_ARC_KIOSK_APP;
  else
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_REGULAR;

  if (primary_user) {
    chromeos::LoginState::Get()->SetLoggedInStateAndPrimaryUser(
        logged_in_state, login_user_type, primary_user->username_hash());
  } else {
    chromeos::LoginState::Get()->SetLoggedInState(logged_in_state,
                                                  login_user_type);
  }
}

}  // namespace chrome_user_manager_util
}  // namespace chromeos
