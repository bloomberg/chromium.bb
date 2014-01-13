// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/brightness/brightness_controller_chromeos.h"

#include "base/metrics/user_metrics.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/base/accelerators/accelerator.h"

namespace ash {
namespace system {

bool BrightnessControllerChromeos::HandleBrightnessDown(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_BRIGHTNESS_DOWN)
    base::RecordAction(
        base::UserMetricsAction("Accel_BrightnessDown_F6"));

  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      DecreaseScreenBrightness(true);
  return true;
}

bool BrightnessControllerChromeos::HandleBrightnessUp(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_BRIGHTNESS_UP)
    base::RecordAction(base::UserMetricsAction("Accel_BrightnessUp_F7"));

  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      IncreaseScreenBrightness();
  return true;
}

void BrightnessControllerChromeos::SetBrightnessPercent(double percent,
                                                        bool gradual) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      SetScreenBrightnessPercent(percent, gradual);
}

void BrightnessControllerChromeos::GetBrightnessPercent(
    const base::Callback<void(double)>& callback) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      GetScreenBrightnessPercent(callback);
}

}  // namespace system
}  // namespace ash
