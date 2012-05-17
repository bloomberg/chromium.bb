// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/brightness_observer.h"

#include "ash/shell.h"
#include "ash/wm/power_button_controller.h"
#include "chrome/browser/extensions/system/system_api.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

BrightnessObserver::BrightnessObserver() {
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
}

BrightnessObserver::~BrightnessObserver() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
}

void BrightnessObserver::BrightnessChanged(int level, bool user_initiated) {
  extensions::DispatchBrightnessChangedEvent(level, user_initiated);
  ash::Shell::GetInstance()->power_button_controller()->
      OnScreenBrightnessChanged(static_cast<double>(level));
}

}  // namespace chromeos
