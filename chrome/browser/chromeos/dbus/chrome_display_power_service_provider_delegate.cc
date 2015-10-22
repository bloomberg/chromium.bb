// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/chrome_display_power_service_provider_delegate.h"

#include "ash/shell.h"
#include "ash/wm/screen_dimmer.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/display/chromeos/display_configurator.h"

namespace chromeos {

ChromeDisplayPowerServiceProviderDelegate::
ChromeDisplayPowerServiceProviderDelegate() {
}

ChromeDisplayPowerServiceProviderDelegate::
~ChromeDisplayPowerServiceProviderDelegate() {
}

void ChromeDisplayPowerServiceProviderDelegate::SetDisplayPower(
    DisplayPowerState power_state,
    const ResponseCallback& callback) {
  // Turning displays off when the device becomes idle or on just before
  // we suspend may trigger a mouse move, which would then be incorrectly
  // reported as user activity.  Let the UserActivityDetector
  // know so that it can ignore such events.
  ui::UserActivityDetector::Get()->OnDisplayPowerChanging();

  ash::Shell::GetInstance()->display_configurator()->SetDisplayPower(
      power_state, ui::DisplayConfigurator::kSetDisplayPowerNoFlags, callback);
}

void ChromeDisplayPowerServiceProviderDelegate::SetDimming(bool dimmed) {
  ash::ScreenDimmer::GetForRoot()->SetDimming(dimmed);
}

}  // namespace chromeos
