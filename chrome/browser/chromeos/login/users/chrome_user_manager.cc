// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/login_state.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"

namespace chromeos {
namespace {

bool IsManagedSessionEnabled(const user_manager::User* active_user) {
  // If the service doesn't exist or the policy is not set, enable managed
  // session by default.
  const bool managed_session_enabled_by_default = true;

  policy::DeviceLocalAccountPolicyService* service =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetDeviceLocalAccountPolicyService();
  if (!service)
    return managed_session_enabled_by_default;

  const policy::PolicyMap::Entry* entry =
      service->GetBrokerForUser(active_user->GetAccountId().GetUserEmail())
          ->core()
          ->store()
          ->policy_map()
          .Get(policy::key::kDeviceLocalAccountManagedSessionEnabled);

  if (!entry)
    return managed_session_enabled_by_default;

  return entry && entry->value && entry->value->GetBool();
}

}  // namespace

ChromeUserManager::ChromeUserManager(
    scoped_refptr<base::TaskRunner> task_runner)
    : UserManagerBase(task_runner) {}

ChromeUserManager::~ChromeUserManager() {}

bool ChromeUserManager::IsCurrentUserNew() const {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chromeos::switches::kForceFirstRunUI))
    return true;

  return UserManagerBase::IsCurrentUserNew();
}

void ChromeUserManager::UpdateLoginState(const user_manager::User* active_user,
                                         const user_manager::User* primary_user,
                                         bool is_current_user_owner) const {
  using chromeos::LoginState;
  if (!LoginState::IsInitialized())
    return;  // LoginState may be uninitialized in tests.

  chromeos::LoginState::LoggedInState logged_in_state;
  logged_in_state = active_user ? chromeos::LoginState::LOGGED_IN_ACTIVE
                                : chromeos::LoginState::LOGGED_IN_NONE;

  chromeos::LoginState::LoggedInUserType login_user_type;
  if (logged_in_state == chromeos::LoginState::LOGGED_IN_NONE) {
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_NONE;
  } else if (is_current_user_owner) {
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_OWNER;
  } else if (active_user->GetType() == user_manager::USER_TYPE_GUEST) {
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_GUEST;
  } else if (active_user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    login_user_type =
        IsManagedSessionEnabled(active_user)
            ? chromeos::LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT_MANAGED
            : chromeos::LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT;
  } else if (active_user->GetType() == user_manager::USER_TYPE_SUPERVISED) {
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_SUPERVISED;
  } else if (active_user->GetType() == user_manager::USER_TYPE_KIOSK_APP) {
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_KIOSK_APP;
  } else if (active_user->GetType() == user_manager::USER_TYPE_ARC_KIOSK_APP) {
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_ARC_KIOSK_APP;
  } else {
    login_user_type = chromeos::LoginState::LOGGED_IN_USER_REGULAR;
  }

  if (primary_user) {
    LoginState::Get()->SetLoggedInStateAndPrimaryUser(
        logged_in_state, login_user_type, primary_user->username_hash());
  } else {
    LoginState::Get()->SetLoggedInState(logged_in_state, login_user_type);
  }
}

bool ChromeUserManager::GetPlatformKnownUserId(
    const std::string& user_email,
    const std::string& gaia_id,
    AccountId* out_account_id) const {
  if (user_email == user_manager::kStubUserEmail) {
    *out_account_id = user_manager::StubAccountId();
    return true;
  }

  if (user_email == user_manager::kStubAdUserEmail) {
    *out_account_id = user_manager::StubAdAccountId();
    return true;
  }

  if (user_email == user_manager::kGuestUserName) {
    *out_account_id = user_manager::GuestAccountId();
    return true;
  }

  return false;
}

// static
ChromeUserManager* ChromeUserManager::Get() {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  return user_manager ? static_cast<ChromeUserManager*>(user_manager) : NULL;
}

// static
user_manager::UserList
ChromeUserManager::GetUsersAllowedAsSupervisedUserManagers(
    const user_manager::UserList& user_list) {
  user_manager::UserList result;
  for (user_manager::User* user : user_list) {
    if (user->GetType() == user_manager::USER_TYPE_REGULAR)
      result.push_back(user);
  }
  return result;
}

}  // namespace chromeos
