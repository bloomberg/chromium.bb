// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/timezone_clock_updater.h"

#include "chrome/browser/chromeos/status/clock_menu_button.h"
#include "chrome/browser/chromeos/system/timezone_settings.h"

TimezoneClockUpdater::TimezoneClockUpdater(ClockMenuButton* button)
    : button_(button) {
  chromeos::system::TimezoneSettings::GetInstance()->AddObserver(this);
}

TimezoneClockUpdater::~TimezoneClockUpdater() {
  chromeos::system::TimezoneSettings::GetInstance()->RemoveObserver(this);
}

void TimezoneClockUpdater::TimezoneChanged(const icu::TimeZone& timezone) {
  button_->UpdateText();
}
