// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/low_battery_observer.h"

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/time_format.h"
#include "grit/generated_resources.h"

namespace chromeos {

LowBatteryObserver::LowBatteryObserver(Profile* profile)
  : notification_(profile, "battery.chromeos",
                  l10n_util::GetStringUTF16(IDS_LOW_BATTERY_TITLE)),
    remaining_(0) {}

LowBatteryObserver::~LowBatteryObserver() {
  Hide();
}

void LowBatteryObserver::PowerChanged(PowerLibrary* object) {
  const int limit = 15;  // Notification will show when remaining number
                         // of minutes is <= limit

  base::TimeDelta remaining = object->battery_time_to_empty();
  bool line_power = object->line_power_on();

  // remaining time of zero means still calculating, this is denoted by
  // TimeDelta().

  // This is a simple state machine with two states and three edges:
  // States: visible_, !visible_
  // Edges: hide: is visible_ to !visible_ triggered if we transition
  //          to line_power, we're calculating our time remaining,
  //          or our time remaining has climbed higher than
  //          limit (either by reduced useor an undected transition to/from
  //          line_power).
  //        update: is visible_ to _visible triggered when we didn't hide
  //          and the minutes remaining changed from what's shown.
  //        show: is !visible_ to visible_ triggered when we're on battery,
  //          we know the remaining time, and that time is less than limit.

  if (notification_.visible()) {
    if (line_power || remaining == base::TimeDelta()
        || remaining.InMinutes() > limit) {
      Hide();
    } else if (remaining.InMinutes() != remaining_) {
      Show(remaining);
    }
  } else {
    if (!line_power && remaining != base::TimeDelta()
        && remaining.InMinutes() <= limit) {
      Show(remaining);
    }
  }
}

void LowBatteryObserver::Show(base::TimeDelta remaining) {
  notification_.Show(l10n_util::GetStringFUTF16(IDS_LOW_BATTERY_MESSAGE,
      WideToUTF16(TimeFormat::TimeRemaining(remaining))));
  remaining_ = remaining.InMinutes();
}

void LowBatteryObserver::Hide() {
  notification_.Hide();
}

}  // namespace chromeos

