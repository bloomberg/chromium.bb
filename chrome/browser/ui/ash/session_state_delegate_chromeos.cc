// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_state_delegate_chromeos.h"

#include "ash/session_state_observer.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_adding_screen.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "google_apis/gaia/gaia_auth_util.h"

SessionStateDelegateChromeos::SessionStateDelegateChromeos() {
  chromeos::UserManager::Get()->AddSessionStateObserver(this);
}

SessionStateDelegateChromeos::~SessionStateDelegateChromeos() {
}

int SessionStateDelegateChromeos::GetMaximumNumberOfLoggedInUsers() const {
  return 3;
}

int SessionStateDelegateChromeos::NumberOfLoggedInUsers() const {
  return chromeos::UserManager::Get()->GetLoggedInUsers().size();
}

bool SessionStateDelegateChromeos::IsActiveUserSessionStarted() const {
  return chromeos::UserManager::Get()->IsSessionStarted();
}

bool SessionStateDelegateChromeos::CanLockScreen() const {
  return chromeos::UserManager::Get()->CanCurrentUserLock();
}

bool SessionStateDelegateChromeos::IsScreenLocked() const {
  return chromeos::ScreenLocker::default_screen_locker() &&
         chromeos::ScreenLocker::default_screen_locker()->locked();
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

const base::string16 SessionStateDelegateChromeos::GetUserDisplayName(
    ash::MultiProfileIndex index) const {
  DCHECK_LT(index, NumberOfLoggedInUsers());
  return chromeos::UserManager::Get()->
             GetLRULoggedInUsers()[index]->display_name();
}

const std::string SessionStateDelegateChromeos::GetUserEmail(
    ash::MultiProfileIndex index) const {
  DCHECK_LT(index, NumberOfLoggedInUsers());
  return chromeos::UserManager::Get()->
             GetLRULoggedInUsers()[index]->display_email();
}

const gfx::ImageSkia& SessionStateDelegateChromeos::GetUserImage(
    ash::MultiProfileIndex index) const {
  DCHECK_LT(index, NumberOfLoggedInUsers());
  return chromeos::UserManager::Get()->GetLRULoggedInUsers()[index]->image();
}

void SessionStateDelegateChromeos::GetLoggedInUsers(ash::UserIdList* users) {
  const chromeos::UserList& logged_in_users =
      chromeos::UserManager::Get()->GetLoggedInUsers();
  for (chromeos::UserList::const_iterator it = logged_in_users.begin();
       it != logged_in_users.end(); ++it) {
    const chromeos::User* user = (*it);
    users->push_back(user->email());
  }
}

void SessionStateDelegateChromeos::SwitchActiveUser(
    const std::string& user_email) {
  // The user_id can be a display email which might be capitalized and has dots.
  chromeos::UserManager::Get()->SwitchActiveUser(
      gaia::CanonicalizeEmail(gaia::SanitizeEmail(user_email)));
}

void SessionStateDelegateChromeos::AddSessionStateObserver(
    ash::SessionStateObserver* observer) {
  session_state_observer_list_.AddObserver(observer);
}

void SessionStateDelegateChromeos::RemoveSessionStateObserver(
    ash::SessionStateObserver* observer) {
  session_state_observer_list_.RemoveObserver(observer);
}

void SessionStateDelegateChromeos::ActiveUserChanged(
    const chromeos::User* active_user) {
  FOR_EACH_OBSERVER(ash::SessionStateObserver,
                    session_state_observer_list_,
                    ActiveUserChanged(active_user->email()));
}
