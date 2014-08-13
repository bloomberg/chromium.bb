// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_state_delegate_chromeos.h"

#include "ash/multi_profile_uma.h"
#include "ash/session/session_state_observer.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/login/login_state.h"
#include "components/user_manager/user.h"
#include "google_apis/gaia/gaia_auth_util.h"

SessionStateDelegateChromeos::SessionStateDelegateChromeos()
    : session_state_(SESSION_STATE_LOGIN_PRIMARY) {
  chromeos::UserManager::Get()->AddSessionStateObserver(this);
  chromeos::UserAddingScreen::Get()->AddObserver(this);

  // LoginState is not initialized in unit_tests.
  if (chromeos::LoginState::IsInitialized()) {
    chromeos::LoginState::Get()->AddObserver(this);
    SetSessionState(chromeos::LoginState::Get()->IsUserLoggedIn() ?
        SESSION_STATE_ACTIVE : SESSION_STATE_LOGIN_PRIMARY, true);
  }
}

SessionStateDelegateChromeos::~SessionStateDelegateChromeos() {
  chromeos::UserManager::Get()->RemoveSessionStateObserver(this);
  chromeos::UserAddingScreen::Get()->RemoveObserver(this);

  // LoginState is not initialized in unit_tests.
  if (chromeos::LoginState::IsInitialized())
    chromeos::LoginState::Get()->RemoveObserver(this);
}

content::BrowserContext* SessionStateDelegateChromeos::GetBrowserContextByIndex(
    ash::MultiProfileIndex index) {
  DCHECK_LT(index, NumberOfLoggedInUsers());
  user_manager::User* user =
      chromeos::UserManager::Get()->GetLRULoggedInUsers()[index];
  DCHECK(user);
  return chromeos::ProfileHelper::Get()->GetProfileByUserUnsafe(user);
}

content::BrowserContext*
SessionStateDelegateChromeos::GetBrowserContextForWindow(
    aura::Window* window) {
  const std::string& user_id =
      chrome::MultiUserWindowManager::GetInstance()->GetWindowOwner(window);
  const user_manager::User* user =
      chromeos::UserManager::Get()->FindUser(user_id);
  DCHECK(user);
  return chromeos::ProfileHelper::Get()->GetProfileByUserUnsafe(user);
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
  return chromeos::UserManager::Get()->GetLoggedInUsers().size();
}

bool SessionStateDelegateChromeos::IsActiveUserSessionStarted() const {
  return chromeos::UserManager::Get()->IsSessionStarted();
}

bool SessionStateDelegateChromeos::CanLockScreen() const {
  const user_manager::UserList unlock_users =
      chromeos::UserManager::Get()->GetUnlockUsers();
  return !unlock_users.empty();
}

bool SessionStateDelegateChromeos::IsScreenLocked() const {
  return chromeos::ScreenLocker::default_screen_locker() &&
         chromeos::ScreenLocker::default_screen_locker()->locked();
}

bool SessionStateDelegateChromeos::ShouldLockScreenBeforeSuspending() const {
  const user_manager::UserList logged_in_users =
      chromeos::UserManager::Get()->GetLoggedInUsers();
  for (user_manager::UserList::const_iterator it = logged_in_users.begin();
       it != logged_in_users.end();
       ++it) {
    user_manager::User* user = (*it);
    Profile* profile =
        chromeos::ProfileHelper::Get()->GetProfileByUserUnsafe(user);
    if (profile->GetPrefs()->GetBoolean(prefs::kEnableAutoScreenLock))
      return true;
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
  bool has_login_manager = CommandLine::ForCurrentProcess()->HasSwitch(
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
    ash::MultiProfileIndex index) const {
  DCHECK_LT(index, NumberOfLoggedInUsers());
  return chromeos::UserManager::Get()->GetLRULoggedInUsers()[index];
}

const user_manager::UserInfo* SessionStateDelegateChromeos::GetUserInfo(
    content::BrowserContext* context) const {
  DCHECK(context);
  return chromeos::ProfileHelper::Get()->GetUserByProfile(
      Profile::FromBrowserContext(context));
}

bool SessionStateDelegateChromeos::ShouldShowAvatar(
    aura::Window* window) const {
  return chrome::MultiUserWindowManager::GetInstance()->
      ShouldShowAvatar(window);
}

void SessionStateDelegateChromeos::SwitchActiveUser(
    const std::string& user_id) {
  // Disallow switching to an already active user since that might crash.
  // Also check that we got a user id and not an email address.
  DCHECK_EQ(user_id,
            gaia::CanonicalizeEmail(gaia::SanitizeEmail(user_id)));
  if (user_id == chromeos::UserManager::Get()->GetActiveUser()->email())
    return;
  chromeos::UserManager::Get()->SwitchActiveUser(user_id);
}

void SessionStateDelegateChromeos::CycleActiveUser(CycleUser cycle_user) {
  // Make sure there is a user to switch to.
  if (NumberOfLoggedInUsers() <= 1)
    return;

  const user_manager::UserList& logged_in_users =
      chromeos::UserManager::Get()->GetLoggedInUsers();

  std::string user_id = chromeos::UserManager::Get()->GetActiveUser()->email();

  // Get an iterator positioned at the active user.
  user_manager::UserList::const_iterator it;
  for (it = logged_in_users.begin();
       it != logged_in_users.end(); ++it) {
    if ((*it)->email() == user_id)
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
        user_id = (*logged_in_users.begin())->email();
      else
        user_id = (*it)->email();
      break;
    case CYCLE_TO_PREVIOUS_USER:
      if (it == logged_in_users.begin())
        it = logged_in_users.end();
      user_id = (*(--it))->email();
      break;
  }

  // Switch using the transformed |user_id|.
  chromeos::UserManager::Get()->SwitchActiveUser(user_id);
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
  FOR_EACH_OBSERVER(ash::SessionStateObserver,
                    session_state_observer_list_,
                    ActiveUserChanged(active_user->email()));
}

void SessionStateDelegateChromeos::UserAddedToSession(
    const user_manager::User* added_user) {
  FOR_EACH_OBSERVER(ash::SessionStateObserver,
                    session_state_observer_list_,
                    UserAddedToSession(added_user->email()));
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
