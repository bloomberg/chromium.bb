// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/brightness_observer.h"

#include "ash/shell.h"
#include "ash/wm/power_button_controller.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/extensions/system/system_api.h"
#include "chrome/browser/lifetime/application_lifetime.h"
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

  // When the user is idle, the power manager dims the screen, turns off the
  // screen and eventually locks the screen (if screen lock on idle and suspend
  // is enabled). For Public Accounts, the session should be terminated instead
  // as soon as the screen turns off.
  // This implementation will be superseded after a revamp of the power manager
  // is complete, see crbug.com/161267.
  if (UserManager::Get()->IsLoggedInAsPublicAccount() &&
      !level && !user_initiated) {
    browser::AttemptUserExit();
  }
}

}  // namespace chromeos
