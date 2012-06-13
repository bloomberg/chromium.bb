// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/screen_dimming_observer.h"

#include "ash/shell.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

ScreenDimmingObserver::ScreenDimmingObserver() {
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
}

ScreenDimmingObserver::~ScreenDimmingObserver() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
}

void ScreenDimmingObserver::ScreenDimmingRequested(ScreenDimmingState state) {
  ash::Shell::GetInstance()->SetDimming(
      state == PowerManagerClient::Observer::SCREEN_DIMMING_IDLE);
}

}  // namespace chromeos
