// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/power_button_controller_delegate_chromeos.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

namespace chromeos {

void PowerButtonControllerDelegateChromeos::RequestLockScreen() {
  // If KioskMode is enabled, if the user attempts to lock the screen via
  // the power button, we instead want to log the user out. This seemed to
  // be the most acceptable replacement for the lock action of the power
  // button for Kiosk mode users.
  if (KioskModeSettings::Get()->IsKioskModeEnabled()) {
    browser::AttemptUserExit();
    return;
  }

  DBusThreadManager::Get()->GetPowerManagerClient()->
      NotifyScreenLockRequested();
}

void PowerButtonControllerDelegateChromeos::RequestShutdown() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestShutdown();
}

}  // namespace chromeos
