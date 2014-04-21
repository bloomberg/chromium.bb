// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/extension_system_event_observer.h"

#include "chrome/browser/extensions/api/system_private/system_private_api.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

ExtensionSystemEventObserver::ExtensionSystemEventObserver() {
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
  DBusThreadManager::Get()->GetSessionManagerClient()->AddObserver(this);
}

ExtensionSystemEventObserver::~ExtensionSystemEventObserver() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
  DBusThreadManager::Get()->GetSessionManagerClient()->RemoveObserver(this);
}

void ExtensionSystemEventObserver::BrightnessChanged(int level,
                                                     bool user_initiated) {
  extensions::DispatchBrightnessChangedEvent(level, user_initiated);
}

void ExtensionSystemEventObserver::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  extensions::DispatchWokeUpEvent();
}

void ExtensionSystemEventObserver::ScreenIsUnlocked() {
  extensions::DispatchScreenUnlockedEvent();
}

}  // namespace chromeos
