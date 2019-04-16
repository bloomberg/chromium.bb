// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/scheduler_utils.h"

namespace notifications {

bool ToLocalHour(int hour,
                 const base::Time& today,
                 int day_delta,
                 base::Time* out) {
  DCHECK_GE(hour, 0);
  DCHECK_LE(hour, 23);
  DCHECK(out);

  // Gets the local time at |hour| in yesterday.
  base::Time another_day = today + base::TimeDelta::FromDays(day_delta);
  base::Time::Exploded another_day_exploded;
  another_day.LocalExplode(&another_day_exploded);
  another_day_exploded.hour = hour;
  another_day_exploded.minute = 0;
  another_day_exploded.second = 0;
  another_day_exploded.millisecond = 0;

  // Converts local exploded time to time stamp.
  return base::Time::FromLocalExploded(another_day_exploded, out);
}

}  // namespace notifications
