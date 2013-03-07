// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/resume_observer.h"

#include "ash/shell.h"
#include "chrome/browser/extensions/api/system_private/system_private_api.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/display/output_configurator.h"

namespace chromeos {

ResumeObserver::ResumeObserver() {
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
}

ResumeObserver::~ResumeObserver() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
}

void ResumeObserver::SystemResumed(const base::TimeDelta& sleep_duration) {
  extensions::DispatchWokeUpEvent();
  ash::Shell::GetInstance()->output_configurator()->ResumeDisplays();
}

}  // namespace chromeos
