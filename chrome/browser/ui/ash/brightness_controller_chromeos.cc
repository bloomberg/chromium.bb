// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/brightness_controller_chromeos.h"

#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/user_metrics.h"
#include "ui/base/accelerators/accelerator.h"

bool BrightnessController::HandleBrightnessDown(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_BRIGHTNESS_DOWN)
    content::RecordAction(
        content::UserMetricsAction("Accel_BrightnessDown_F6"));

  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      DecreaseScreenBrightness(true);
  return true;
}

bool BrightnessController::HandleBrightnessUp(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_BRIGHTNESS_UP)
    content::RecordAction(content::UserMetricsAction("Accel_BrightnessUp_F7"));

  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      IncreaseScreenBrightness();
  return true;
}

void BrightnessController::SetBrightnessPercent(double percent, bool gradual) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      SetScreenBrightnessPercent(percent, gradual);
}

void BrightnessController::GetBrightnessPercent(
    const base::Callback<void(double)>& callback) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      GetScreenBrightnessPercent(callback);
}
