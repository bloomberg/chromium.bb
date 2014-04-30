// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/power_event_observer.h"

#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/wm/power_button_controller.h"
#include "base/prefs/pref_service.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/display/chromeos/display_configurator.h"
#include "ui/wm/core/user_activity_detector.h"

namespace ash {

PowerEventObserver::PowerEventObserver()
    : screen_locked_(false) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      AddObserver(this);
  chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
      AddObserver(this);
}

PowerEventObserver::~PowerEventObserver() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RemoveObserver(this);
  chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
      RemoveObserver(this);
}

void PowerEventObserver::BrightnessChanged(int level, bool user_initiated) {
  Shell::GetInstance()->power_button_controller()->OnScreenBrightnessChanged(
      static_cast<double>(level));
}

void PowerEventObserver::SuspendImminent() {
  Shell* shell = Shell::GetInstance();
  SessionStateDelegate* delegate = shell->session_state_delegate();

  // If the lock-before-suspending pref is set, get a callback to block
  // suspend and ask the session manager to lock the screen.
  if (!screen_locked_ && delegate->ShouldLockScreenBeforeSuspending() &&
      delegate->CanLockScreen()) {
    screen_lock_callback_ = chromeos::DBusThreadManager::Get()->
        GetPowerManagerClient()->GetSuspendReadinessCallback();
    VLOG(1) << "Requesting screen lock from PowerEventObserver";
    chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
        RequestLockScreen();
  }

  shell->user_activity_detector()->OnDisplayPowerChanging();
  shell->display_configurator()->SuspendDisplays();
}

void PowerEventObserver::SuspendDone(const base::TimeDelta& sleep_duration) {
  Shell::GetInstance()->display_configurator()->ResumeDisplays();
  Shell::GetInstance()->system_tray_notifier()->NotifyRefreshClock();
}

void PowerEventObserver::ScreenIsLocked() {
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

void PowerEventObserver::ScreenIsUnlocked() {
  screen_locked_ = false;
}

}  // namespace ash
