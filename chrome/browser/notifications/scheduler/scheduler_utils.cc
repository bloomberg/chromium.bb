// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/scheduler_utils.h"

namespace notifications {

bool ToLocalYesterdayHour(int hour, const base::Time& today, base::Time* out) {
  DCHECK_GE(hour, 0);
  DCHECK_LE(hour, 23);
  DCHECK(out);

  // Gets the local time at |hour| in yesterday.
  base::Time yesterday = today - base::TimeDelta::FromDays(1);
  base::Time::Exploded yesterday_exploded;
  yesterday.LocalExplode(&yesterday_exploded);
  yesterday_exploded.hour = hour;
  yesterday_exploded.minute = 0;
  yesterday_exploded.second = 0;
  yesterday_exploded.millisecond = 0;

  // Converts local exploded time to time stamp.
  return base::Time::FromLocalExploded(yesterday_exploded, out);
}

}  // namespace notifications
