// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_WEEKLY_TIME_WEEKLY_TIME_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_WEEKLY_TIME_WEEKLY_TIME_H_

#include <memory>

#include "base/strings/string16.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/proto/chrome_device_policy.pb.h"

namespace policy {

// WeeklyTime class contains day of week and time. Day of week is number from 1
// to 7 (1 = Monday, 2 = Tuesday, etc.) Time is in milliseconds from the
// beginning of the day. WeeklyTime's timezone should be interpreted to be in
// UTC.
class WeeklyTime {
 public:
  WeeklyTime(int day_of_week, int milliseconds);

  WeeklyTime(const WeeklyTime& rhs);

  WeeklyTime& operator=(const WeeklyTime& rhs);

  // Return DictionaryValue in format:
  // { "day_of_week" : int # value is from 1 to 7 (1 = Monday, 2 = Tuesday,
  // etc.)
  //   "time" : int # in milliseconds from the beginning of the day.
  // }
  std::unique_ptr<base::DictionaryValue> ToValue() const;

  int day_of_week() const { return day_of_week_; }

  int milliseconds() const { return milliseconds_; }

  // Return duration from |start| till |end| week times. |end| time
  // is always after |start| time. It's possible because week time is cyclic.
  // (i.e. [Friday 17:00, Monday 9:00) )
  base::TimeDelta GetDurationTo(const WeeklyTime& other) const;

  // Add milliseconds to WeeklyTime.
  WeeklyTime AddMilliseconds(int milliseconds) const;

  // Convert WeeklyTime to a string that is in local time and localized to the
  // system's language and time display settings. The output is in the format
  // "EEEE jj:mm a" E.g. for |day_of_week_| = 4 and |milliseconds_| = 5 hours
  // (in ms) then the output should be "Thursday 5:00 AM" in an US locale in UTC
  // timezone. Similarly, the output will be "Donnerstag 05:00" in a german
  // locale in UTC timezone (there may be slight changes due to different
  // standards in different locales).
  base::string16 ToLocalizedString() const;

  // Return WeeklyTime structure from WeeklyTimeProto. Return nullptr if
  // WeeklyTime structure isn't correct.
  static std::unique_ptr<WeeklyTime> ExtractFromProto(
      const enterprise_management::WeeklyTimeProto& container);

  // Return current time in WeeklyTime structure.
  static WeeklyTime GetCurrentWeeklyTime(base::Clock* clock);

 private:
  // Number of weekday (1 = Monday, 2 = Tuesday, etc.)
  int day_of_week_;

  // Time of day in milliseconds from the beginning of the day.
  int milliseconds_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_WEEKLY_TIME_WEEKLY_TIME_H_
