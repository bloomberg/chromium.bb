
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_WEEKLY_TIME_TIME_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_WEEKLY_TIME_TIME_UTILS_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base {
class Clock;
class TimeDelta;
}  // namespace base

namespace policy {

class WeeklyTime;
class WeeklyTimeInterval;

namespace weekly_time_utils {

// Put time in milliseconds which is added to local time to get GMT time to
// |offset| considering daylight from |clock|. Return true if there was no
// error.
bool GetOffsetFromTimezoneToGmt(const std::string& timezone,
                                base::Clock* clock,
                                int* offset);

bool GetOffsetFromTimezoneToGmt(const icu::TimeZone& timezone,
                                base::Clock* clock,
                                int* offset);

// The output is in the format "EEEE jj:mm a".
// Example: For a WeeklyTime(4 /* day_of_week */,
//                           5 * 3600*1000 /* milliseconds */,
//                           0 /* timezone_offset */)
// the output should be "Thursday 5:00 AM" in an US locale in GMT timezone.
// Similarly, the output will be "Donnerstag 05:00" in a German locale in a GMT
// timezone (there may be slight changes in formatting due to different
// standards in different locales).
base::string16 WeeklyTimeToLocalizedString(const WeeklyTime& weekly_time,
                                           base::Clock* clock);

// Convert time intervals from |timezone| to GMT timezone. Timezone agnostic
// intervals are not supported.
std::vector<WeeklyTimeInterval> ConvertIntervalsToGmt(
    const std::vector<WeeklyTimeInterval>& intervals);

// Return duration till next weekly time interval.
base::TimeDelta GetDeltaTillNextTimeInterval(
    const WeeklyTime& current_time,
    const std::vector<WeeklyTimeInterval>& weekly_time_intervals);

}  // namespace weekly_time_utils
}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_WEEKLY_TIME_TIME_UTILS_H_
