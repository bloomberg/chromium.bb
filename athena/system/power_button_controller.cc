// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/system/power_button_controller.h"

#include "chromeos/dbus/dbus_thread_manager.h"

namespace athena {

PowerButtonController::PowerButtonController()
    : brightness_is_zero_(false),
      shutdown_requested_(false) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
}

PowerButtonController::~PowerButtonController() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
}

void PowerButtonController::BrightnessChanged(int level, bool user_initiated) {
  if (brightness_is_zero_)
    zero_brightness_end_time_ = base::TimeTicks::Now();
  brightness_is_zero_ = (level == 0);
}

void PowerButtonController::PowerButtonEventReceived(
    bool down,
    const base::TimeTicks& details) {
  // Avoid requesting a shutdown if the power button is pressed while the screen
  // is off (http://crbug.com/128451)
  base::TimeDelta time_since_zero_brightness = brightness_is_zero_ ?
      base::TimeDelta() : (base::TimeTicks::Now() - zero_brightness_end_time_);
  const int kShortTimeMs = 10;
  if (time_since_zero_brightness.InMilliseconds() <= kShortTimeMs)
    return;

  if (down && !shutdown_requested_) {
    shutdown_requested_ = true;
    chromeos::DBusThreadManager::Get()
        ->GetPowerManagerClient()
        ->RequestShutdown();
  }
}

}  // namespace athena
