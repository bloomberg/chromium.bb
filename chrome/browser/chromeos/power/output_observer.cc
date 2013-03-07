// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/output_observer.h"

#include "ash/shell.h"
#include "ash/wm/user_activity_detector.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/display/output_configurator.h"

namespace chromeos {

OutputObserver::OutputObserver() {
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
}

OutputObserver::~OutputObserver() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
}

void OutputObserver::ScreenPowerSet(bool power_on, bool all_displays) {
  // Turning displays off when the device becomes idle or on just before we
  // suspend may trigger a mouse move, which would then be incorrectly
  // reported as user activity.  Let the UserActivityDetector know so that
  // it can ignore such events.
  ash::Shell::GetInstance()->user_activity_detector()->OnDisplayPowerChanging();

  DisplayPowerState state = DISPLAY_POWER_ALL_ON;
  if (power_on && all_displays)
    state = DISPLAY_POWER_ALL_ON;
  else if (power_on && !all_displays)
    state = DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF;
  else if (!power_on && all_displays)
    state = DISPLAY_POWER_ALL_OFF;
  else if (!power_on && !all_displays)
    state = DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON;

  ash::Shell::GetInstance()->output_configurator()->SetDisplayPower(
      state, false);
}

}  // namespace chromeos
