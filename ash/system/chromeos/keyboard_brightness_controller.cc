// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/keyboard_brightness_controller.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shell.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/base/accelerators/accelerator.h"

namespace ash {

bool KeyboardBrightnessController::HandleKeyboardBrightnessDown(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_BRIGHTNESS_DOWN) {
    Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        UMA_ACCEL_KEYBOARD_BRIGHTNESS_DOWN_F6);
  }

  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      DecreaseKeyboardBrightness();
  return true;
}

bool KeyboardBrightnessController::HandleKeyboardBrightnessUp(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_BRIGHTNESS_UP) {
    Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        UMA_ACCEL_KEYBOARD_BRIGHTNESS_UP_F7);
  }

  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      IncreaseKeyboardBrightness();
  return true;
}

}  // namespace ash
