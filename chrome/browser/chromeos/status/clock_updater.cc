// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/clock_updater.h"

#include "chrome/browser/chromeos/status/clock_menu_button.h"
#include "chrome/browser/chromeos/system/timezone_settings.h"
#include "chromeos/dbus/dbus_thread_manager.h"

ClockUpdater::ClockUpdater(ClockMenuButton* button)
    : button_(button) {
  chromeos::system::TimezoneSettings::GetInstance()->AddObserver(this);
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      AddObserver(this);
}

ClockUpdater::~ClockUpdater() {
  chromeos::system::TimezoneSettings::GetInstance()->RemoveObserver(this);
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RemoveObserver(this);
}

void ClockUpdater::TimezoneChanged(const icu::TimeZone& timezone) {
  button_->UpdateText();
}

void ClockUpdater::SystemResumed() {
  button_->UpdateText();
}
