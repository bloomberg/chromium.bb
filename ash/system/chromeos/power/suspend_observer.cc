// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/suspend_observer.h"

#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/wm/user_activity_detector.h"
#include "base/prefs/pref_service.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/display/output_configurator.h"

namespace ash {
namespace internal {

SuspendObserver::SuspendObserver()
    : power_client_(
          chromeos::DBusThreadManager::Get()->GetPowerManagerClient()),
      session_client_(
          chromeos::DBusThreadManager::Get()->GetSessionManagerClient()),
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
  Shell* shell = Shell::GetInstance();
  SessionStateDelegate* delegate = shell->session_state_delegate();

  // If the lock-before-suspending pref is set, get a callback to block
  // suspend and ask the session manager to lock the screen.
  if (!screen_locked_ && delegate->ShouldLockScreenBeforeSuspending() &&
      delegate->CanLockScreen()) {
    screen_lock_callback_ = power_client_->GetSuspendReadinessCallback();
    VLOG(1) << "Requesting screen lock from SuspendObserver";
    session_client_->RequestLockScreen();
  }

  shell->user_activity_detector()->OnDisplayPowerChanging();
  shell->output_configurator()->SuspendDisplays();
}

void SuspendObserver::ScreenIsLocked() {
  screen_locked_ = true;

  // Stop blocking suspend after the screen is locked.
  if (!screen_lock_callback_.is_null()) {
    VLOG(1) << "Screen locked due to suspend";
    // Run the callback asynchronously.  ScreenIsLocked() is currently
    // called asynchronously after RequestLockScreen(), but this guards
    // against it being made synchronous later.
    base::MessageLoop::current()->PostTask(FROM_HERE, screen_lock_callback_);
    screen_lock_callback_.Reset();
  } else {
    VLOG(1) << "Screen locked without suspend";
  }
}

void SuspendObserver::ScreenIsUnlocked() {
  screen_locked_ = false;
}

}  // namespace internal
}  // namespace ash
