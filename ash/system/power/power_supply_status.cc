// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_supply_status.h"

#include "base/format_macros.h"
#include "base/stringprintf.h"

namespace ash {

#if !defined(OS_CHROMEOS)
PowerSupplyStatus::PowerSupplyStatus()
    : line_power_on(false),
      battery_is_present(false),
      battery_is_full(false),
      battery_seconds_to_empty(0),
      battery_seconds_to_full(0),
      averaged_battery_time_to_empty(0),
      averaged_battery_time_to_full(0),
      battery_percentage(0),
      is_calculating_battery_time(false),
      battery_energy_rate(0) {}

std::string PowerSupplyStatus::ToString() const {
  std::string result;
  base::StringAppendF(&result,
                      "line_power_on = %s ",
                      line_power_on ? "true" : "false");
  base::StringAppendF(&result,
                      "battery_is_present = %s ",
                      battery_is_present ? "true" : "false");
  base::StringAppendF(&result,
                      "battery_is_full = %s ",
                      battery_is_full ? "true" : "false");
  base::StringAppendF(&result,
                      "battery_percentage = %f ",
                      battery_percentage);
  base::StringAppendF(&result,
                      "battery_seconds_to_empty = %"PRId64" ",
                      battery_seconds_to_empty);
  base::StringAppendF(&result,
                      "battery_seconds_to_full = %"PRId64" ",
                      battery_seconds_to_full);
  base::StringAppendF(&result,
                      "averaged_battery_time_to_empty = %"PRId64" ",
                      averaged_battery_time_to_empty);
  base::StringAppendF(&result,
                      "averaged_battery_time_to_full = %"PRId64" ",
                      averaged_battery_time_to_full);
  base::StringAppendF(&result,
                      "is_calculating_battery_time = %s ",
                      is_calculating_battery_time ? "true" : "false");
  base::StringAppendF(&result,
                      "battery_energy_rate = %f ",
                      battery_energy_rate);
  return result;
}
#endif  // !defined(OS_CHROMEOS)

}  // namespace ash
