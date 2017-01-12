// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_controller_client.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/session_types.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/multi_user/user_switch_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/resource/resource_bundle.h"

using session_manager::SessionManager;
using user_manager::UserManager;
using user_manager::User;
using user_manager::UserList;

namespace {

SessionControllerClient* g_instance = nullptr;

uint32_t GetSessionId(const User* user) {
  const UserList logged_in_users = UserManager::Get()->GetLoggedInUsers();
  // TODO(xiyuan): Update with real session id when user session tracking
  //     code is moved from UserManager to SessionManager.
  for (size_t i = 0; i < logged_in_users.size(); ++i) {
    if (logged_in_users[i] == user)
      return i + 1;
  }

  NOTREACHED();
  return 0u;
}

ash::mojom::UserSessionPtr UserToUserSession(const User& user) {
  ash::mojom::UserSessionPtr session = ash::mojom::UserSession::New();
  session->session_id = GetSessionId(&user);
  session->type = user.GetType();
  session->account_id = user.GetAccountId();
  session->display_name = base::UTF16ToUTF8(user.display_name());
  session->display_email = user.display_email();

  // TODO(xiyuan): Observe user image change and update.
  //     Tracked in http://crbug.com/670422
  // TODO(xiyuan): Support multiple scale factor.
  session->avatar = *user.GetImage().bitmap();
  if (session->avatar.isNull()) {
    session->avatar = *ResourceBundle::GetSharedInstance()
                           .GetImageSkiaNamed(IDR_PROFILE_PICTURE_LOADING)
                           ->bitmap();
  }

  return session;
}

void DoSwitchUser(const AccountId& account_id) {
  UserManager::Get()->SwitchActiveUser(account_id);
}

}  // namespace

SessionControllerClient::SessionControllerClient() : binding_(this) {
  SessionManager::Get()->AddObserver(this);
  UserManager::Get()->AddSessionStateObserver(this);

  ConnectToSessionControllerAndSetClient();
  SendSessionInfoIfChanged();
  // User sessions and their order will be sent via UserSessionStateObserver
  // even for crash-n-restart.

  DCHECK(!g_instance);
  g_instance = this;
}

SessionControllerClient::~SessionControllerClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;

  SessionManager::Get()->RemoveObserver(this);
  UserManager::Get()->RemoveSessionStateObserver(this);
}

void SessionControllerClient::RequestLockScreen() {
  DoLockScreen();
}

void SessionControllerClient::SwitchActiveUser(const AccountId& account_id) {
  DoSwitchActiveUser(account_id);
}

void SessionControllerClient::CycleActiveUser(bool next_user) {
  DoCycleActiveUser(next_user);
}

void SessionControllerClient::ActiveUserChanged(const User* active_user) {
  SendSessionInfoIfChanged();

  // UserAddedToSession is not called for the primary user session so send its
  // meta data here once.
  if (!primary_user_session_sent_ &&
      UserManager::Get()->GetPrimaryUser() == active_user) {
    primary_user_session_sent_ = true;
    SendUserSession(*active_user);
  }

  SendUserSessionOrder();
}

void SessionControllerClient::UserAddedToSession(const User* added_user) {
  SendSessionInfoIfChanged();
  SendUserSession(*added_user);
}

// static
bool SessionControllerClient::CanLockScreen() {
  return !UserManager::Get()->GetUnlockUsers().empty();
}

// static
bool SessionControllerClient::ShouldLockScreenAutomatically() {
  // TODO(xiyuan): Observe prefs::kEnableAutoScreenLock and update ash.
  // Tracked in http://crbug.com/670423
  const UserList logged_in_users = UserManager::Get()->GetLoggedInUsers();
  for (auto* user : logged_in_users) {
    Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
    if (profile &&
        profile->GetPrefs()->GetBoolean(prefs::kEnableAutoScreenLock)) {
      return true;
    }
  }
  return false;
}

// static
ash::AddUserSessionPolicy SessionControllerClient::GetAddUserSessionPolicy() {
  UserManager* const user_manager = UserManager::Get();
  if (user_manager->GetUsersAllowedForMultiProfile().empty())
    return ash::AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS;

  if (chromeos::MultiProfileUserController::GetPrimaryUserPolicy() !=
      chromeos::MultiProfileUserController::ALLOWED) {
    return ash::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER;
  }

  if (UserManager::Get()->GetLoggedInUsers().size() >=
      SessionManager::Get()->GetMaximumNumberOfUserSessions())
    return ash::AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED;

  return ash::AddUserSessionPolicy::ALLOWED;
}

// static
void SessionControllerClient::DoLockScreen() {
  if (!CanLockScreen())
    return;

  VLOG(1) << "Requesting screen lock from SessionControllerClient";
  chromeos::DBusThreadManager::Get()
      ->GetSessionManagerClient()
      ->RequestLockScreen();
}

// static
void SessionControllerClient::DoSwitchActiveUser(const AccountId& account_id) {
  // Disallow switching to an already active user since that might crash.
  // Also check that we got a user id and not an email address.
  DCHECK_EQ(
      account_id.GetUserEmail(),
      gaia::CanonicalizeEmail(gaia::SanitizeEmail(account_id.GetUserEmail())));
  if (account_id == UserManager::Get()->GetActiveUser()->GetAccountId())
    return;

  TrySwitchingActiveUser(base::Bind(&DoSwitchUser, account_id));
}

// static
void SessionControllerClient::DoCycleActiveUser(bool next_user) {
  const UserList& logged_in_users = UserManager::Get()->GetLoggedInUsers();
  if (logged_in_users.size() <= 1)
    return;

  AccountId account_id = UserManager::Get()->GetActiveUser()->GetAccountId();

  // Get an iterator positioned at the active user.
  auto it = std::find_if(logged_in_users.begin(), logged_in_users.end(),
                         [account_id](const User* user) {
                           return user->GetAccountId() == account_id;
                         });

  // Active user not found.
  if (it == logged_in_users.end())
    return;

  // Get the user's email to select, wrapping to the start/end of the list if
  // necessary.
  if (next_user) {
    if (++it == logged_in_users.end())
      account_id = (*logged_in_users.begin())->GetAccountId();
    else
      account_id = (*it)->GetAccountId();
  } else {
    if (it == logged_in_users.begin())
      it = logged_in_users.end();
    account_id = (*(--it))->GetAccountId();
  }

  DoSwitchActiveUser(account_id);
}

// static
void SessionControllerClient::FlushForTesting() {
  g_instance->session_controller_.FlushForTesting();
}

void SessionControllerClient::OnSessionStateChanged() {
  SendSessionInfoIfChanged();
}

void SessionControllerClient::ConnectToSessionControllerAndSetClient() {
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash_util::GetAshServiceName(), &session_controller_);

  // Set as |session_controller_|'s client.
  session_controller_->SetClient(binding_.CreateInterfacePtrAndBind());
}

void SessionControllerClient::SendSessionInfoIfChanged() {
  SessionManager* const session_manager = SessionManager::Get();

  ash::mojom::SessionInfoPtr info = ash::mojom::SessionInfo::New();
  info->max_users = session_manager->GetMaximumNumberOfUserSessions();
  info->can_lock_screen = CanLockScreen();
  info->should_lock_screen_automatically = ShouldLockScreenAutomatically();
  info->add_user_session_policy = GetAddUserSessionPolicy();
  info->state = session_manager->session_state();

  if (info != last_sent_session_info_) {
    last_sent_session_info_ = info->Clone();
    session_controller_->SetSessionInfo(std::move(info));
  }
}

void SessionControllerClient::SendUserSession(const User& user) {
  session_controller_->UpdateUserSession(UserToUserSession(user));
}

void SessionControllerClient::SendUserSessionOrder() {
  UserManager* const user_manager = UserManager::Get();

  const UserList logged_in_users = user_manager->GetLoggedInUsers();
  std::vector<uint32_t> user_session_ids;
  for (auto* user : user_manager->GetLRULoggedInUsers())
    user_session_ids.push_back(GetSessionId(user));

  session_controller_->SetUserSessionOrder(user_session_ids);
}
