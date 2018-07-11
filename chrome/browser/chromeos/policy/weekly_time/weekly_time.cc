// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/weekly_time/weekly_time.h"

#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"

namespace em = enterprise_management;

namespace policy {
namespace {

constexpr base::TimeDelta kWeek = base::TimeDelta::FromDays(7);
constexpr base::TimeDelta kDay = base::TimeDelta::FromDays(1);
constexpr base::TimeDelta kHour = base::TimeDelta::FromHours(1);
constexpr base::TimeDelta kMinute = base::TimeDelta::FromMinutes(1);
constexpr base::TimeDelta kSecond = base::TimeDelta::FromSeconds(1);

const char* kFormatWeekdayHourMinute = "EEEE jj:mm a";

}  // namespace

WeeklyTime::WeeklyTime(int day_of_week, int milliseconds)
    : day_of_week_(day_of_week), milliseconds_(milliseconds) {
  DCHECK_GT(day_of_week, 0);
  DCHECK_LE(day_of_week, 7);
  DCHECK_GE(milliseconds, 0);
  DCHECK_LT(milliseconds, kDay.InMilliseconds());
}

WeeklyTime::WeeklyTime(const WeeklyTime& rhs) = default;

WeeklyTime& WeeklyTime::operator=(const WeeklyTime& rhs) = default;

std::unique_ptr<base::DictionaryValue> WeeklyTime::ToValue() const {
  auto weekly_time = std::make_unique<base::DictionaryValue>();
  weekly_time->SetInteger("day_of_week", day_of_week_);
  weekly_time->SetInteger("time", milliseconds_);
  return weekly_time;
}

base::TimeDelta WeeklyTime::GetDurationTo(const WeeklyTime& other) const {
  int duration = (other.day_of_week() - day_of_week_) * kDay.InMilliseconds() +
                 other.milliseconds() - milliseconds_;
  if (duration < 0)
    duration += kWeek.InMilliseconds();
  return base::TimeDelta::FromMilliseconds(duration);
}

WeeklyTime WeeklyTime::AddMilliseconds(int milliseconds) const {
  milliseconds %= kWeek.InMilliseconds();
  // Make |milliseconds| positive number (add number of milliseconds per week)
  // for easier evaluation.
  milliseconds += kWeek.InMilliseconds();
  int shifted_milliseconds = milliseconds_ + milliseconds;
  // Get milliseconds from the start of the day.
  int result_milliseconds = shifted_milliseconds % kDay.InMilliseconds();
  int day_offset = shifted_milliseconds / kDay.InMilliseconds();
  // Convert day of week considering week is cyclic. +/- 1 is
  // because day of week is from 1 to 7.
  int result_day_of_week = (day_of_week_ + day_offset - 1) % 7 + 1;
  return WeeklyTime(result_day_of_week, result_milliseconds);
}

base::string16 WeeklyTime::ToLocalizedString() const {
  // Clock with the current time.
  base::Clock* default_clock = base::DefaultClock::GetInstance();
  WeeklyTime now_weekly_time = GetCurrentWeeklyTime(default_clock);
  // Offset the current time so that its day of the week and time match
  // |day_of_week| and |milliseconds_|.
  base::Time offset_time =
      default_clock->Now() + now_weekly_time.GetDurationTo(*this);
  return base::TimeFormatWithPattern(offset_time, kFormatWeekdayHourMinute);
}

// static
WeeklyTime WeeklyTime::GetCurrentWeeklyTime(base::Clock* clock) {
  base::Time::Exploded exploded;
  clock->Now().UTCExplode(&exploded);
  int day_of_week = exploded.day_of_week == 0 ? 7 : exploded.day_of_week;
  int milliseconds = exploded.hour * kHour.InMilliseconds() +
                     exploded.minute * kMinute.InMilliseconds() +
                     exploded.second * kSecond.InMilliseconds() +
                     exploded.millisecond;
  return WeeklyTime(day_of_week, milliseconds);
}

// static
std::unique_ptr<WeeklyTime> WeeklyTime::ExtractFromProto(
    const em::WeeklyTimeProto& container) {
  if (!container.has_day_of_week() ||
      container.day_of_week() == em::WeeklyTimeProto::DAY_OF_WEEK_UNSPECIFIED) {
    LOG(ERROR) << "Day of week is absent or unspecified.";
    return nullptr;
  }
  if (!container.has_time()) {
    LOG(ERROR) << "Time is absent.";
    return nullptr;
  }
  int time_of_day = container.time();
  if (!(time_of_day >= 0 && time_of_day < kDay.InMilliseconds())) {
    LOG(ERROR) << "Invalid time value: " << time_of_day
               << ", the value should be in [0; " << kDay.InMilliseconds()
               << ").";
    return nullptr;
  }
  return std::make_unique<WeeklyTime>(container.day_of_week(), time_of_day);
}

}  // namespace policy
