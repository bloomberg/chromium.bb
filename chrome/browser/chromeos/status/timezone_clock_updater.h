// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_TIMEZONE_CLOCK_UPDATER_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_TIMEZONE_CLOCK_UPDATER_H_
#pragma once

#include "chrome/browser/chromeos/system/timezone_settings.h"

class ClockMenuButton;

// TimezoneClockUpdater is responsible for updating a ClockMenuButton when the
// timezone changes.
class TimezoneClockUpdater
    : public chromeos::system::TimezoneSettings::Observer {
 public:
  explicit TimezoneClockUpdater(ClockMenuButton* button);
  virtual ~TimezoneClockUpdater();

  // TimezoneSettings::Observer overrides:
  virtual void TimezoneChanged(const icu::TimeZone& timezone) OVERRIDE;

 private:
  ClockMenuButton* button_;

  DISALLOW_COPY_AND_ASSIGN(TimezoneClockUpdater);
};

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_TIMEZONE_CLOCK_UPDATER_H_
