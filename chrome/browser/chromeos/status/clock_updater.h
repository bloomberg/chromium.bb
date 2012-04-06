// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_CLOCK_UPDATER_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_CLOCK_UPDATER_H_
#pragma once

#include "chrome/browser/chromeos/system/timezone_settings.h"
#include "chromeos/dbus/power_manager_client.h"

class ClockMenuButton;

// ClockUpdater is responsible for updating a ClockMenuButton when:
// 1. the timezone changes.
// 2. the system resumes from sleep.
class ClockUpdater : public chromeos::system::TimezoneSettings::Observer,
                     public chromeos::PowerManagerClient::Observer {
 public:
  explicit ClockUpdater(ClockMenuButton* button);
  virtual ~ClockUpdater();

  // TimezoneSettings::Observer overrides:
  virtual void TimezoneChanged(const icu::TimeZone& timezone) OVERRIDE;

  // PowerManagerClient::Observer overrides:
  virtual void SystemResumed() OVERRIDE;

 private:
  ClockMenuButton* button_;

  DISALLOW_COPY_AND_ASSIGN(ClockUpdater);
};

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_CLOCK_UPDATER_H_
