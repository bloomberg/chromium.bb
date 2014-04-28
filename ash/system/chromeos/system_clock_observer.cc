// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/system_clock_observer.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace ash {

SystemClockObserver::SystemClockObserver() {
  chromeos::DBusThreadManager::Get()->GetSystemClockClient()
      ->AddObserver(this);
  chromeos::system::TimezoneSettings::GetInstance()->AddObserver(this);
  can_set_time_ =
      chromeos::DBusThreadManager::Get()->GetSystemClockClient()->CanSetTime();
}

SystemClockObserver::~SystemClockObserver() {
  chromeos::DBusThreadManager::Get()->GetSystemClockClient()
      ->RemoveObserver(this);
  chromeos::system::TimezoneSettings::GetInstance()->RemoveObserver(this);
}

void SystemClockObserver::SystemClockUpdated() {
  Shell::GetInstance()->system_tray_notifier()->NotifySystemClockTimeUpdated();
}

void SystemClockObserver::SystemClockCanSetTimeChanged(bool can_set_time) {
  can_set_time_ = can_set_time;
  Shell::GetInstance()->system_tray_notifier()
      ->NotifySystemClockCanSetTimeChanged(can_set_time_);
}

void SystemClockObserver::TimezoneChanged(const icu::TimeZone& timezone) {
  Shell::GetInstance()->system_tray_notifier()->NotifyRefreshClock();
}

}  // namespace ash
