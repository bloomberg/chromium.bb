// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/input/power_button_controller.h"

#include "athena/input/public/accelerator_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/events/event_constants.h"

namespace athena {
namespace {

// The amount of time that the power button must be held to be
// treated as long press.
const int kLongPressTimeoutMs = 1000;

enum {
  CMD_DEBUG_POWER_BUTTON_PRESSED,
  CMD_DEBUG_POWER_BUTTON_RELEASED,
};

}  // namespace

PowerButtonController::PowerButtonController()
    : power_button_timeout_ms_(kLongPressTimeoutMs),
      brightness_is_zero_(false) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
}

PowerButtonController::~PowerButtonController() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
}

void PowerButtonController::AddPowerButtonObserver(
    PowerButtonObserver* observer) {
  observers_.AddObserver(observer);
}

void PowerButtonController::RemovePowerButtonObserver(
    PowerButtonObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PowerButtonController::InstallAccelerators() {
  const AcceleratorData accelerator_data[] = {
      {TRIGGER_ON_PRESS,
       ui::VKEY_P,
       ui::EF_ALT_DOWN,
       CMD_DEBUG_POWER_BUTTON_PRESSED,
       AF_DEBUG | AF_NON_AUTO_REPEATABLE},
      {TRIGGER_ON_RELEASE,
       ui::VKEY_P,
       ui::EF_ALT_DOWN,
       CMD_DEBUG_POWER_BUTTON_RELEASED,
       AF_DEBUG},
  };
  AcceleratorManager::Get()->RegisterAccelerators(
      accelerator_data, arraysize(accelerator_data), this);
}

int PowerButtonController::SetPowerButtonTimeoutMsForTest(int timeout) {
  int old_timeout = power_button_timeout_ms_;
  power_button_timeout_ms_ = timeout;
  return old_timeout;
}

void PowerButtonController::BrightnessChanged(int level, bool user_initiated) {
  if (brightness_is_zero_)
    zero_brightness_end_time_ = base::TimeTicks::Now();
  brightness_is_zero_ = (level == 0);
}

void PowerButtonController::PowerButtonEventReceived(
    bool down,
    const base::TimeTicks& timestamp) {
  // Ignore power button pressed while the screen is off
  // (http://crbug.com/128451).
  // TODO(oshima): This needs to be revisited for athena.
  base::TimeDelta time_since_zero_brightness =
      brightness_is_zero_
          ? base::TimeDelta()
          : (base::TimeTicks::Now() - zero_brightness_end_time_);
  const int kShortTimeMs = 10;
  if (time_since_zero_brightness.InMilliseconds() <= kShortTimeMs)
    return;

  if (down) {
    FOR_EACH_OBSERVER(PowerButtonObserver,
                      observers_,
                      OnPowerButtonStateChanged(PowerButtonObserver::PRESSED));
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMilliseconds(kLongPressTimeoutMs),
                 this,
                 &PowerButtonController::NotifyLongPress);
  } else {
    FOR_EACH_OBSERVER(PowerButtonObserver,
                      observers_,
                      OnPowerButtonStateChanged(PowerButtonObserver::RELEASED));
    timer_.Stop();
  }
}

bool PowerButtonController::IsCommandEnabled(int command_id) const {
  return true;
}

bool PowerButtonController::OnAcceleratorFired(
    int command_id,
    const ui::Accelerator& accelerator) {
  switch (command_id) {
    case CMD_DEBUG_POWER_BUTTON_PRESSED:
      PowerButtonEventReceived(true, base::TimeTicks());
      break;
    case CMD_DEBUG_POWER_BUTTON_RELEASED:
      PowerButtonEventReceived(false, base::TimeTicks());
      break;
  }
  return true;
}

void PowerButtonController::NotifyLongPress() {
  FOR_EACH_OBSERVER(
      PowerButtonObserver,
      observers_,
      OnPowerButtonStateChanged(PowerButtonObserver::LONG_PRESSED));
}

}  // namespace
