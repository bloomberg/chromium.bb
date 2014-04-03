// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_SYSTEM_CLOCK_OBSERVER_H_
#define ASH_SYSTEM_CHROMEOS_SYSTEM_CLOCK_OBSERVER_H_

#include "chromeos/dbus/system_clock_client.h"
#include "chromeos/settings/timezone_settings.h"

namespace ash {

class SystemClockObserver
    : public chromeos::SystemClockClient::Observer,
      public chromeos::system::TimezoneSettings::Observer {
 public:
  SystemClockObserver();
  virtual ~SystemClockObserver();

  // chromeos::SystemClockClient::Observer
  virtual void SystemClockUpdated() OVERRIDE;

  // chromeos::system::TimezoneSettings::Observer
  virtual void TimezoneChanged(const icu::TimeZone& timezone) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemClockObserver);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_SYSTEM_CLOCK_OBSERVER_H_
