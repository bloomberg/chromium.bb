// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_state_delegate.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"

SessionStateDelegate::SessionStateDelegate() {
}

SessionStateDelegate::~SessionStateDelegate() {
}

int SessionStateDelegate::GetMaximumNumberOfLoggedInUsers() const {
  return 3;
}

int SessionStateDelegate::NumberOfLoggedInUsers() const {
  return chromeos::UserManager::Get()->GetLoggedInUsers().size();
}

bool SessionStateDelegate::IsActiveUserSessionStarted() const {
  return chromeos::UserManager::Get()->IsSessionStarted();
}

bool SessionStateDelegate::CanLockScreen() const {
  return chromeos::UserManager::Get()->CanCurrentUserLock();
}

bool SessionStateDelegate::IsScreenLocked() const {
  return chromeos::ScreenLocker::default_screen_locker() &&
         chromeos::ScreenLocker::default_screen_locker()->locked();
}

void SessionStateDelegate::LockScreen() {
  if (!CanLockScreen())
    return;

  // TODO(antrim): Additional logging for http://crbug.com/173178.
  LOG(WARNING) << "Requesting screen lock from SessionStateDelegate";
  chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
      RequestLockScreen();
}

void SessionStateDelegate::UnlockScreen() {
  // This is used only for testing thus far.
  NOTIMPLEMENTED();
}

const base::string16 SessionStateDelegate::GetUserDisplayName(
    ash::MultiProfileIndex index) const {
  DCHECK_LT(index, NumberOfLoggedInUsers());
  return chromeos::UserManager::Get()->
             GetLRULoggedInUsers()[index]->display_name();
}

const std::string SessionStateDelegate::GetUserEmail(
    ash::MultiProfileIndex index) const {
  DCHECK_LT(index, NumberOfLoggedInUsers());
  return chromeos::UserManager::Get()->
             GetLRULoggedInUsers()[index]->display_email();
}

const gfx::ImageSkia& SessionStateDelegate::GetUserImage(
    ash::MultiProfileIndex index) const {
  DCHECK_LT(index, NumberOfLoggedInUsers());
  return chromeos::UserManager::Get()->GetLRULoggedInUsers()[index]->image();
}

void SessionStateDelegate::GetLoggedInUsers(
    ash::UserEmailList* users) {
  const chromeos::UserList& logged_in_users =
      chromeos::UserManager::Get()->GetLoggedInUsers();
  for (chromeos::UserList::const_iterator it = logged_in_users.begin();
       it != logged_in_users.end(); ++it) {
    const chromeos::User* user = (*it);
    users->push_back(user->email());
  }
}

void SessionStateDelegate::SwitchActiveUser(const std::string& email) {
  chromeos::UserManager::Get()->SwitchActiveUser(email);
}
