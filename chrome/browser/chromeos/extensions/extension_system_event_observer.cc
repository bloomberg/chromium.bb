// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/extension_system_event_observer.h"

#include "chrome/browser/extensions/api/system_private/system_private_api.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/session_manager/core/session_manager.h"

namespace chromeos {

ExtensionSystemEventObserver::ExtensionSystemEventObserver()
    : screen_locked_(session_manager::SessionManager::Get()->IsScreenLocked()) {
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
  session_manager::SessionManager::Get()->AddObserver(this);
}

ExtensionSystemEventObserver::~ExtensionSystemEventObserver() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void ExtensionSystemEventObserver::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  const bool user_initiated =
      change.cause() ==
      power_manager::BacklightBrightnessChange_Cause_USER_REQUEST;
  extensions::DispatchBrightnessChangedEvent(change.percent(), user_initiated);
}

void ExtensionSystemEventObserver::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  extensions::DispatchWokeUpEvent();
}

void ExtensionSystemEventObserver::OnSessionStateChanged() {
  const bool was_locked = screen_locked_;
  screen_locked_ = session_manager::SessionManager::Get()->IsScreenLocked();
  if (was_locked && !screen_locked_)
    extensions::DispatchScreenUnlockedEvent();
}

}  // namespace chromeos
