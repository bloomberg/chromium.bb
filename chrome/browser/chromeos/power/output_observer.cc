// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/output_observer.h"

#include "ash/shell.h"
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
  ash::Shell::GetInstance()->output_configurator()->
      ScreenPowerSet(power_on, all_displays);
}

}  // namespace chromeos
