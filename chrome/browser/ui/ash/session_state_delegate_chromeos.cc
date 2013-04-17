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

bool SessionStateDelegate::HasActiveUser() const {
  return chromeos::UserManager::Get()->IsUserLoggedIn();
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
