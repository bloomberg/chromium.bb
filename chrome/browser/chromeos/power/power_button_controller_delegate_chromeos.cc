// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/power_button_controller_delegate_chromeos.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"

namespace chromeos {

void PowerButtonControllerDelegateChromeos::RequestLockScreen() {
  DBusThreadManager::Get()->GetPowerManagerClient()->
      NotifyScreenLockRequested();
}

void PowerButtonControllerDelegateChromeos::RequestShutdown() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestShutdown();
}

}  // namespace chromeos
