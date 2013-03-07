// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/suspend_observer.h"

#include "ash/shell.h"
#include "ash/wm/user_activity_detector.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/extensions/api/system_private/system_private_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/display/output_configurator.h"

namespace chromeos {

SuspendObserver::SuspendObserver()
    : power_client_(DBusThreadManager::Get()->GetPowerManagerClient()),
      session_client_(DBusThreadManager::Get()->GetSessionManagerClient()),
      screen_locked_(false) {
  power_client_->AddObserver(this);
  session_client_->AddObserver(this);
}

SuspendObserver::~SuspendObserver() {
  session_client_->RemoveObserver(this);
  session_client_ = NULL;
  power_client_->RemoveObserver(this);
  power_client_ = NULL;
}

void SuspendObserver::SuspendImminent() {
  // If the lock-before-suspending pref is set, get a callback to block
  // suspend and ask the session manager to lock the screen.
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  if (profile && profile->GetPrefs()->GetBoolean(prefs::kEnableScreenLock) &&
      UserManager::Get()->CanCurrentUserLock() && !screen_locked_) {
    screen_lock_callback_ = power_client_->GetSuspendReadinessCallback();
    // TODO(antrim) : additional logging for crbug/173178
    LOG(WARNING) << "Requesting screen lock from SuspendObserver";
    session_client_->RequestLockScreen();
  }

  ash::Shell::GetInstance()->user_activity_detector()->OnDisplayPowerChanging();
  ash::Shell::GetInstance()->output_configurator()->SuspendDisplays();
}

void SuspendObserver::ScreenIsLocked() {
  screen_locked_ = true;

  // Stop blocking suspend after the screen is locked.
  if (!screen_lock_callback_.is_null()) {
    // Run the callback asynchronously.  ScreenIsLocked() is currently
    // called asynchronously after RequestLockScreen(), but this guards
    // against it being made synchronous later.
    MessageLoop::current()->PostTask(FROM_HERE, screen_lock_callback_);
    screen_lock_callback_.Reset();
  }
}

void SuspendObserver::ScreenIsUnlocked() {
  screen_locked_ = false;
}

}  // namespace chromeos
