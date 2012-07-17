// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/keyboard_brightness_controller_chromeos.h"

#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/user_metrics.h"
#include "ui/base/accelerators/accelerator.h"

bool KeyboardBrightnessController::HandleKeyboardBrightnessDown(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_F6) {
    content::RecordAction(
        content::UserMetricsAction("Accel_KeyboardBrightnessDown_F6"));
  }

  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      DecreaseKeyboardBrightness();
  return true;
}

bool KeyboardBrightnessController::HandleKeyboardBrightnessUp(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_F7) {
    content::RecordAction(
        content::UserMetricsAction("Accel_KeyboardBrightnessUp_F7"));
  }

  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      IncreaseKeyboardBrightness();
  return true;
}
