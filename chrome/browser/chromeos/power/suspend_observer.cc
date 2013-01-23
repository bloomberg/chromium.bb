// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/suspend_observer.h"
#include "chrome/browser/extensions/system/system_api.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/display/output_configurator.h"

namespace chromeos {

SuspendObserver::SuspendObserver() {
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
}

SuspendObserver::~SuspendObserver() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
}

void SuspendObserver::SuspendImminent() {
  ash::Shell::GetInstance()->output_configurator()->SuspendDisplays();
}

}  // namespace chromeos
