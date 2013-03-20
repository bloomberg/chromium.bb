// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_SUPPLY_STATUS_H_
#define ASH_SYSTEM_POWER_POWER_SUPPLY_STATUS_H_

#include <string>

#include "ash/ash_export.h"
#include "base/basictypes.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/power_supply_status.h"
#endif

namespace ash {

#if defined(OS_CHROMEOS)
typedef chromeos::PowerSupplyStatus PowerSupplyStatus;
#else
// Define local struct when not building for Chrome OS.
struct ASH_EXPORT PowerSupplyStatus {
  bool line_power_on;

  bool battery_is_present;
  bool battery_is_full;

  // Time in seconds until the battery is empty or full, 0 for unknown.
  int64 battery_seconds_to_empty;
  int64 battery_seconds_to_full;
  int64 averaged_battery_time_to_empty;
  int64 averaged_battery_time_to_full;

  double battery_percentage;

  bool is_calculating_battery_time;

  // Rate of charge/discharge of the battery, in W.
  double battery_energy_rate;

  PowerSupplyStatus();
  std::string ToString() const;
};
#endif  // defined(OS_CHROMEOS)

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_SUPPLY_STATUS_H_
