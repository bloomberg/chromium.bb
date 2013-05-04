// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_POWER_SUPPLY_STATUS_H_
#define CHROMEOS_DBUS_POWER_SUPPLY_STATUS_H_

#include <string>

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

struct CHROMEOS_EXPORT PowerSupplyStatus {
  enum BatteryState {
    // Line power is connected and the battery is full or being charged.
    CHARGING = 0,

    // Line power is disconnected.
    DISCHARGING = 1,

    // Line power is connected and the battery isn't full, but it also
    // isn't charging (or discharging).  This can occur if the EC believes
    // that the battery isn't authentic and chooses to run directly off
    // line power.
    NEITHER_CHARGING_NOR_DISCHARGING = 2,

    // A USB power source (DCP, CDP, or ACA) is connected.  The battery may
    // be charging or discharging depending on the negotiated current.
    CONNECTED_TO_USB = 3,
  };

  bool line_power_on;

  bool battery_is_present;
  bool battery_is_full;

  // Time in seconds until the battery is empty or full, 0 for unknown.
  int64 battery_seconds_to_empty;
  int64 battery_seconds_to_full;

  double battery_percentage;

  bool is_calculating_battery_time;

  BatteryState battery_state;

  PowerSupplyStatus();
  std::string ToString() const;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_POWER_SUPPLY_STATUS_H_
