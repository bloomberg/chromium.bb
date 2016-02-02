// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_state_delegate_chromeos.h"

#include "ash/content/shell_content_state.h"
#include "ash/multi_profile_uma.h"
#include "ash/session/session_state_observer.h"
#include "ash/system/chromeos/multi_user/user_switch_util.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/session_util.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/login/login_state.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_info.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/gfx/image/image_skia.h"

SessionStateDelegateChromeos::SessionStateDelegateChromeos()
    : session_state_(SESSION_STATE_LOGIN_PRIMARY) {
  user_manager::UserManager::Get()->AddSessionStateObserver(this);
  chromeos::UserAddingScreen::Get()->AddObserver(this);

  // LoginState is not initialized in unit_tests.
  if (chromeos::LoginState::IsInitialized()) {
    chromeos::LoginState::Get()->AddObserver(this);
    SetSessionState(chromeos::LoginState::Get()->IsUserLoggedIn() ?
        SESSION_STATE_ACTIVE : SESSION_STATE_LOGIN_PRIMARY, true);
  }
}

SessionStateDelegateChromeos::~SessionStateDelegateChromeos() {
  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
  chromeos::UserAddingScreen::Get()->RemoveObserver(this);

  // LoginState is not initialized in unit_tests.
  if (chromeos::LoginState::IsInitialized())
    chromeos::LoginState::Get()->RemoveObserver(this);
}

int SessionStateDelegateChromeos::GetMaximumNumberOfLoggedInUsers() const {
  // We limit list of logged in users to 10 due to memory constraints.
  // Note that 10 seems excessive, but we want to test how many users are
  // actually added to a session.
  // TODO(nkostylev): Adjust this limitation based on device capabilites.
  // http://crbug.com/230865
  return 10;
}

int SessionStateDelegateChromeos::NumberOfLoggedInUsers() const {
  return user_manager::UserManager::Get()->GetLoggedInUsers().size();
}

bool SessionStateDelegateChromeos::CanAddUserToMultiProfile(
    AddUserError* error) const {
  if (user_manager::UserManager::Get()
          ->GetUsersAllowedForMultiProfile()
          .size() == 0) {
    if (error)
      *error = ADD_USER_ERROR_OUT_OF_USERS;
    return false;
  }
  return SessionStateDelegate::CanAddUserToMultiProfile(error);
}

bool SessionStateDelegateChromeos::IsActiveUserSessionStarted() const {
  return user_manager::UserManager::Get()->IsSessionStarted();
}

bool SessionStateDelegateChromeos::CanLockScreen() const {
  const user_manager::UserList unlock_users =
      user_manager::UserManager::Get()->GetUnlockUsers();
  return !unlock_users.empty();
}

bool SessionStateDelegateChromeos::IsScreenLocked() const {
  return chromeos::ScreenLocker::default_screen_locker() &&
         chromeos::ScreenLocker::default_screen_locker()->locked();
}

bool SessionStateDelegateChromeos::ShouldLockScreenBeforeSuspending() const {
  const user_manager::UserList logged_in_users =
      user_manager::UserManager::Get()->GetLoggedInUsers();
  for (user_manager::UserList::const_iterator it = logged_in_users.begin();
       it != logged_in_users.end();
       ++it) {
    user_manager::User* user = (*it);
    Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
    if (profile &&
        profile->GetPrefs()->GetBoolean(prefs::kEnableAutoScreenLock)) {
      return true;
    }
  }
  return false;
}

void SessionStateDelegateChromeos::LockScreen() {
  if (!CanLockScreen())
    return;

  VLOG(1) << "Requesting screen lock from SessionStateDelegate";
  chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
      RequestLockScreen();
}

void SessionStateDelegateChromeos::UnlockScreen() {
  // This is used only for testing thus far.
  NOTIMPLEMENTED();
}

bool SessionStateDelegateChromeos::IsUserSessionBlocked() const {
  bool has_login_manager = base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kLoginManager);
  return (has_login_manager && !IsActiveUserSessionStarted()) ||
         IsScreenLocked() ||
         chromeos::UserAddingScreen::Get()->IsRunning();
}

ash::SessionStateDelegate::SessionState
SessionStateDelegateChromeos::GetSessionState() const {
  return session_state_;
}

const user_manager::UserInfo* SessionStateDelegateChromeos::GetUserInfo(
    ash::UserIndex index) const {
  DCHECK_LT(index, NumberOfLoggedInUsers());
  return user_manager::UserManager::Get()->GetLRULoggedInUsers()[index];
}

bool SessionStateDelegateChromeos::ShouldShowAvatar(
    aura::Window* window) const {
  return chrome::MultiUserWindowManager::GetInstance()->
      ShouldShowAvatar(window);
}

gfx::ImageSkia SessionStateDelegateChromeos::GetAvatarImageForWindow(
    aura::Window* window) const {
  content::BrowserContext* context =
      ash::ShellContentState::GetInstance()->GetBrowserContextForWindow(window);
  return GetAvatarImageForContext(context);
}

void SessionStateDelegateChromeos::SwitchActiveUser(
    const AccountId& account_id) {
  // Disallow switching to an already active user since that might crash.
  // Also check that we got a user id and not an email address.
  DCHECK_EQ(
      account_id.GetUserEmail(),
      gaia::CanonicalizeEmail(gaia::SanitizeEmail(account_id.GetUserEmail())));
  if (account_id ==
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId())
    return;
  TryToSwitchUser(account_id);
}

void SessionStateDelegateChromeos::CycleActiveUser(CycleUser cycle_user) {
  // Make sure there is a user to switch to.
  if (NumberOfLoggedInUsers() <= 1)
    return;

  const user_manager::UserList& logged_in_users =
      user_manager::UserManager::Get()->GetLoggedInUsers();

  AccountId account_id =
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();

  // Get an iterator positioned at the active user.
  user_manager::UserList::const_iterator it;
  for (it = logged_in_users.begin();
       it != logged_in_users.end(); ++it) {
    if ((*it)->GetAccountId() == account_id)
      break;
  }

  // Active user not found.
  if (it == logged_in_users.end())
    return;

  // Get the user's email to select, wrapping to the start/end of the list if
  // necessary.
  switch (cycle_user) {
    case CYCLE_TO_NEXT_USER:
      if (++it == logged_in_users.end())
        account_id = (*logged_in_users.begin())->GetAccountId();
      else
        account_id = (*it)->GetAccountId();
      break;
    case CYCLE_TO_PREVIOUS_USER:
      if (it == logged_in_users.begin())
        it = logged_in_users.end();
      account_id = (*(--it))->GetAccountId();
      break;
  }

  // Switch using the transformed |account_id|.
  TryToSwitchUser(account_id);
}

bool SessionStateDelegateChromeos::IsMultiProfileAllowedByPrimaryUserPolicy()
    const {
  return chromeos::MultiProfileUserController::GetPrimaryUserPolicy() ==
         chromeos::MultiProfileUserController::ALLOWED;
}

void SessionStateDelegateChromeos::AddSessionStateObserver(
    ash::SessionStateObserver* observer) {
  session_state_observer_list_.AddObserver(observer);
}

void SessionStateDelegateChromeos::RemoveSessionStateObserver(
    ash::SessionStateObserver* observer) {
  session_state_observer_list_.RemoveObserver(observer);
}

void SessionStateDelegateChromeos::LoggedInStateChanged() {
  SetSessionState(chromeos::LoginState::Get()->IsUserLoggedIn() ?
      SESSION_STATE_ACTIVE : SESSION_STATE_LOGIN_PRIMARY, false);
}

void SessionStateDelegateChromeos::ActiveUserChanged(
    const user_manager::User* active_user) {
  FOR_EACH_OBSERVER(ash::SessionStateObserver, session_state_observer_list_,
                    ActiveUserChanged(active_user->GetAccountId()));
}

void SessionStateDelegateChromeos::UserAddedToSession(
    const user_manager::User* added_user) {
  FOR_EACH_OBSERVER(ash::SessionStateObserver, session_state_observer_list_,
                    UserAddedToSession(added_user->GetAccountId()));
}

void SessionStateDelegateChromeos::OnUserAddingStarted() {
  SetSessionState(SESSION_STATE_LOGIN_SECONDARY, false);
}

void SessionStateDelegateChromeos::OnUserAddingFinished() {
  SetSessionState(SESSION_STATE_ACTIVE, false);
}

void SessionStateDelegateChromeos::SetSessionState(SessionState new_state,
                                                   bool force) {
  if (session_state_ == new_state && !force)
    return;

  session_state_ = new_state;
  NotifySessionStateChanged();
}

void SessionStateDelegateChromeos::NotifySessionStateChanged() {
  FOR_EACH_OBSERVER(ash::SessionStateObserver,
                    session_state_observer_list_,
                    SessionStateChanged(session_state_));
}

void DoSwitchUser(const AccountId& account_id) {
  user_manager::UserManager::Get()->SwitchActiveUser(account_id);
}

void SessionStateDelegateChromeos::TryToSwitchUser(
    const AccountId& account_id) {
  ash::TrySwitchingActiveUser(base::Bind(&DoSwitchUser, account_id));
}
