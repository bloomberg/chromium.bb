// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/low_battery_observer.h"

#include "base/utf_string_conversions.h"
#include "chrome/common/time_format.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

LowBatteryObserver::LowBatteryObserver(Profile* profile)
  : notification_(profile, "battery.chromeos",
                  IDR_NOTIFICATION_LOW_BATTERY,
                  l10n_util::GetStringUTF16(IDS_LOW_BATTERY_TITLE)),
    remaining_(0) {}

LowBatteryObserver::~LowBatteryObserver() {
  Hide();
}

void LowBatteryObserver::PowerChanged(PowerLibrary* object) {
  const int limit_min = 15;   // Notification will show when remaining number
                              // of minutes is <= limit.
  const int limit_max = 30;   // Notification will hid when remaining number
                              // of minutes is > limit_max.
  const int critical = 5;  // Notification will be forced visible if hidden
                           // by user when time remaining <= critical.

  base::TimeDelta remaining = object->battery_time_to_empty();
  int remaining_minutes = remaining.InMinutes();

  // To simplify the logic - we handle the case of calculating the remaining
  // time as if we were on line power.
  // remaining time of zero means still calculating, this is denoted by
  // base::TimeDelta().
  bool line_power = object->line_power_on() || remaining == base::TimeDelta();

  // The urgent flag is used to re-notify the user if the power level
  // goes critical. We only want to do this once even if the time remaining
  // goes back up (so long as it doesn't go above limit_max.
  bool urgent = !line_power &&
      (notification_.urgent() || remaining_minutes <= critical);

  // This is a simple state machine with two states and three edges:
  // States: visible_, !visible_
  // Edges: hide: is visible_ to !visible_ triggered if we transition
  //          to line_power, we're calculating our time remaining,
  //          or our time remaining has climbed higher than
  //          limit_max (either by reduced user an undetected transition
  //          to/from line_power).
  //        update: is visible_ to _visible triggered when we didn't hide
  //          and the minutes remaining changed from what's shown.
  //        show: is !visible_ to visible_ triggered when we're on battery,
  //          we know the remaining time, and that time is less than limit.

  if (notification_.visible()) {
    if (line_power || remaining_minutes > limit_max) {
      Hide();
    } else if (remaining_minutes != remaining_) {
      Show(remaining, urgent);
    }
  } else {
    if (!line_power && remaining_minutes <= limit_min) {
      Show(remaining, urgent);
    }
  }
}

void LowBatteryObserver::Show(base::TimeDelta remaining, bool urgent) {
  notification_.Show(l10n_util::GetStringFUTF16(IDS_LOW_BATTERY_MESSAGE,
      TimeFormat::TimeRemaining(remaining)), urgent, true);
  remaining_ = remaining.InMinutes();
}

void LowBatteryObserver::Hide() {
  notification_.Hide();
}

}  // namespace chromeos
