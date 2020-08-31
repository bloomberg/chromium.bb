// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/power_utils.h"

#include "base/time/time.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace ash {

namespace power_utils {

bool ShouldDisplayBatteryTime(const base::TimeDelta& time) {
  // Put limits on the maximum and minimum battery time-to-full or time-to-empty
  // that should be displayed in the UI. If the current is close to zero,
  // battery time estimates can get very large; avoid displaying these large
  // numbers.
  return time >= base::TimeDelta::FromMinutes(1) &&
         time <= base::TimeDelta::FromDays(1);
}

int GetRoundedBatteryPercent(double battery_percent) {
  // Minimum battery percentage rendered in UI.
  constexpr int kMinBatteryPercent = 1;
  return std::max(kMinBatteryPercent, static_cast<int>(battery_percent + 0.5));
}

void SplitTimeIntoHoursAndMinutes(const base::TimeDelta& time,
                                  int* hours,
                                  int* minutes) {
  DCHECK(hours);
  DCHECK(minutes);
  const int total_minutes = static_cast<int>(time.InSecondsF() / 60 + 0.5);
  *hours = total_minutes / 60;
  *minutes = total_minutes % 60;
}

}  // namespace power_utils

}  // namespace ash
