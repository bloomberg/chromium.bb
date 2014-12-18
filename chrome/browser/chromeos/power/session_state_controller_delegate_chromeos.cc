// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/session_state_controller_delegate_chromeos.h"

#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"

namespace chromeos {

void SessionStateControllerDelegateChromeos::RequestLockScreen() {
  // TODO(antrim) : additional logging for crbug/173178
  LOG(WARNING) << "Requesting screen lock from SessionStateControllerDelegate";
  DBusThreadManager::Get()->GetSessionManagerClient()->RequestLockScreen();
}

void SessionStateControllerDelegateChromeos::RequestRestart() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
}

void SessionStateControllerDelegateChromeos::RequestShutdown() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestShutdown();
}

}  // namespace chromeos
