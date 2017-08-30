// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_off_hours_controller.h"

#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"

namespace em = enterprise_management;

namespace policy {
namespace {

// WeeklyTime class contains day of week and time in milliseconds.
struct WeeklyTime {
  WeeklyTime(int day_of_week, int milliseconds)
      : day_of_week(day_of_week), milliseconds(milliseconds) {}

  std::unique_ptr<base::DictionaryValue> ToValue() const {
    auto weekly_time = base::MakeUnique<base::DictionaryValue>();
    weekly_time->SetInteger("day_of_week", day_of_week);
    weekly_time->SetInteger("time", milliseconds);
    return weekly_time;
  }

  // Number of weekday (1 = Monday, 2 = Tuesday, etc.)
  int day_of_week;
  // Time of day in milliseconds from the beginning of the day.
  int milliseconds;
};

// Time interval struct, interval = [start, end]
struct Interval {
  Interval(const WeeklyTime& start, const WeeklyTime& end)
      : start(start), end(end) {}

  std::unique_ptr<base::DictionaryValue> ToValue() const {
    auto interval = base::MakeUnique<base::DictionaryValue>();
    interval->SetDictionary("start", start.ToValue());
    interval->SetDictionary("end", end.ToValue());
    return interval;
  }

  WeeklyTime start;
  WeeklyTime end;
};

// Get and return WeeklyTime structure from WeeklyTimeProto
// Return nullptr if WeeklyTime structure isn't correct
std::unique_ptr<WeeklyTime> GetWeeklyTime(
    const em::WeeklyTimeProto& container) {
  if (!container.has_day_of_week() ||
      container.day_of_week() == em::WeeklyTimeProto::DAY_OF_WEEK_UNSPECIFIED) {
    LOG(ERROR) << "Day of week in interval is absent or unspecified.";
    return nullptr;
  }
  if (!container.has_time()) {
    LOG(ERROR) << "Time in interval is absent.";
    return nullptr;
  }
  const int kMillisecondsInDay = base::TimeDelta::FromDays(1).InMilliseconds();
  int time_of_day = container.time();
  if (!(time_of_day >= 0 && time_of_day < kMillisecondsInDay)) {
    LOG(ERROR) << "Invalid time value: " << time_of_day
               << ", the value should be in [0; " << kMillisecondsInDay << ").";
    return nullptr;
  }
  return base::MakeUnique<WeeklyTime>(container.day_of_week(), time_of_day);
}

// Get and return list of time intervals from DeviceOffHoursProto structure
std::vector<Interval> GetIntervals(const em::DeviceOffHoursProto& container) {
  std::vector<Interval> intervals;
  for (const auto& entry : container.intervals()) {
    if (!entry.has_start() || !entry.has_end()) {
      LOG(WARNING) << "Skipping interval without start or/and end.";
      continue;
    }
    auto start = GetWeeklyTime(entry.start());
    auto end = GetWeeklyTime(entry.end());
    if (start && end) {
      intervals.push_back(Interval(*start, *end));
    }
  }
  return intervals;
}

std::vector<std::string> GetIgnoredPolicies(
    const em::DeviceOffHoursProto& container) {
  std::vector<std::string> ignored_policies;
  return std::vector<std::string>(container.ignored_policies().begin(),
                                  container.ignored_policies().end());
}

}  // namespace

namespace off_hours {

std::unique_ptr<base::DictionaryValue> ConvertPolicyProtoToValue(
    const em::DeviceOffHoursProto& container) {
  if (!container.has_timezone())
    return nullptr;
  auto off_hours = base::MakeUnique<base::DictionaryValue>();
  off_hours->SetString("timezone", container.timezone());
  std::vector<Interval> intervals = GetIntervals(container);
  auto intervals_value = base::MakeUnique<base::ListValue>();
  for (const auto& interval : intervals) {
    intervals_value->Append(interval.ToValue());
  }
  off_hours->SetList("intervals", std::move(intervals_value));
  std::vector<std::string> ignored_policies = GetIgnoredPolicies(container);
  auto ignored_policies_value = base::MakeUnique<base::ListValue>();
  for (const auto& policy : ignored_policies) {
    ignored_policies_value->AppendString(policy);
  }
  off_hours->SetList("ignored_policies", std::move(ignored_policies_value));
  return off_hours;
}

}  // namespace off_hours
}  // namespace policy
