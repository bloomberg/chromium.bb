// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/resume_observer.h"

#include "chrome/browser/extensions/system/system_api.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/root_power_manager_client.h"

namespace chromeos {

ResumeObserver::ResumeObserver() {
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
  DBusThreadManager::Get()->GetRootPowerManagerClient()->AddObserver(this);
}

ResumeObserver::~ResumeObserver() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
  DBusThreadManager::Get()->GetRootPowerManagerClient()->RemoveObserver(this);
}

void ResumeObserver::SystemResumed(const base::TimeDelta& sleep_duration) {
  extensions::DispatchWokeUpEvent();
}

void ResumeObserver::OnResume(const base::TimeDelta& sleep_duration) {
  SystemResumed(sleep_duration);
}

}  // namespace chromeos
