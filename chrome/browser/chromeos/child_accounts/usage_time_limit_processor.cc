// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usage_time_limit_processor.h"

#include "base/strings/string_number_conversions.h"

namespace chromeos {
namespace internal {
namespace {

constexpr char kOverrideAction[] = "action";
constexpr char kOverrideActionCreatedAt[] = "created_at_millis";
constexpr char kOverrideActionDurationMins[] = "duration_mins";
constexpr char kOverrideActionLock[] = "LOCK";
constexpr char kOverrideActionSpecificData[] = "action_specific_data";
constexpr char kTimeLimitLastUpdatedAt[] = "last_updated_millis";
constexpr char kUsageLimitResetAt[] = "reset_at";
constexpr char kUsageLimitUsageQuota[] = "usage_quota_mins";
constexpr char kWindowLimitEntries[] = "entries";
constexpr char kWindowLimitEntryEffectiveDay[] = "effective_day";
constexpr char kWindowLimitEntryEndsAt[] = "ends_at";
constexpr char kWindowLimitEntryStartsAt[] = "starts_at";
constexpr char kWindowLimitEntryTimeHour[] = "hour";
constexpr char kWindowLimitEntryTimeMinute[] = "minute";
constexpr const char* kTimeLimitWeekdays[] = {
    "sunday",   "monday", "tuesday", "wednesday",
    "thursday", "friday", "saturday"};

}  // namespace

// Transforms the time dictionary sent on the UsageTimeLimit policy to a
// TimeDelta, that represents the distance from midnight.
base::TimeDelta ValueToTimeDelta(const base::Value* policy_time) {
  int hour = policy_time->FindKey(kWindowLimitEntryTimeHour)->GetInt();
  int minute = policy_time->FindKey(kWindowLimitEntryTimeMinute)->GetInt();
  return base::TimeDelta::FromMinutes(hour * 60 + minute);
}

// Transforms weekday strings into the Weekday enum.
Weekday GetWeekday(std::string weekday) {
  std::transform(weekday.begin(), weekday.end(), weekday.begin(), ::tolower);
  for (int i = 0; i < static_cast<int>(Weekday::kCount); i++) {
    if (weekday == kTimeLimitWeekdays[i]) {
      return static_cast<Weekday>(i);
    }
  }

  LOG(ERROR) << "Unexpected weekday " << weekday;
  return Weekday::kSunday;
}

TimeWindowLimitEntry::TimeWindowLimitEntry() = default;

TimeWindowLimitEntry::TimeWindowLimitEntry(TimeWindowLimitEntry&&) = default;

TimeWindowLimitEntry& TimeWindowLimitEntry::operator=(TimeWindowLimitEntry&&) =
    default;

bool TimeWindowLimitEntry::IsOvernight() const {
  return ends_at < starts_at;
}

TimeWindowLimit::TimeWindowLimit(const base::Value& window_limit_dict) {
  if (!window_limit_dict.FindKey(kWindowLimitEntries))
    return;

  for (const base::Value& entry_dict :
       window_limit_dict.FindKey(kWindowLimitEntries)->GetList()) {
    const base::Value* effective_day =
        entry_dict.FindKey(kWindowLimitEntryEffectiveDay);
    const base::Value* starts_at =
        entry_dict.FindKey(kWindowLimitEntryStartsAt);
    const base::Value* ends_at = entry_dict.FindKey(kWindowLimitEntryEndsAt);
    const base::Value* last_updated_value =
        entry_dict.FindKey(kTimeLimitLastUpdatedAt);

    if (!effective_day || !starts_at || !ends_at || !last_updated_value) {
      // Missing information, so this entry will be ignored.
      continue;
    }

    int64_t last_updated;
    if (!base::StringToInt64(last_updated_value->GetString(), &last_updated)) {
      // Cannot process entry without a valid last updated.
      continue;
    }

    TimeWindowLimitEntry entry;
    entry.starts_at = ValueToTimeDelta(starts_at);
    entry.ends_at = ValueToTimeDelta(ends_at);
    entry.last_updated = base::Time::UnixEpoch() +
                         base::TimeDelta::FromMilliseconds(last_updated);

    Weekday weekday = GetWeekday(effective_day->GetString());
    // We only support one time_limit_window per day. If more than one is sent
    // we only use the latest updated.
    if (!entries[weekday] ||
        entries[weekday].value().last_updated < entry.last_updated) {
      entries[weekday] = std::move(entry);
    }
  }
}

TimeWindowLimit::~TimeWindowLimit() = default;

TimeWindowLimit::TimeWindowLimit(TimeWindowLimit&&) = default;

TimeWindowLimit& TimeWindowLimit::operator=(TimeWindowLimit&&) = default;

TimeUsageLimitEntry::TimeUsageLimitEntry() = default;

TimeUsageLimitEntry::TimeUsageLimitEntry(TimeUsageLimitEntry&&) = default;

TimeUsageLimitEntry& TimeUsageLimitEntry::operator=(TimeUsageLimitEntry&&) =
    default;

TimeUsageLimit::TimeUsageLimit(const base::Value& usage_limit_dict)
    // Default reset time is midnight.
    : reset_at(base::TimeDelta::FromMinutes(0)) {
  const base::Value* reset_at_value =
      usage_limit_dict.FindKey(kUsageLimitResetAt);
  if (reset_at_value) {
    reset_at = ValueToTimeDelta(reset_at_value);
  }

  for (const std::string& weekday_key : kTimeLimitWeekdays) {
    if (!usage_limit_dict.FindKey(weekday_key))
      continue;

    const base::Value* entry_dict = usage_limit_dict.FindKey(weekday_key);

    const base::Value* usage_quota = entry_dict->FindKey(kUsageLimitUsageQuota);
    const base::Value* last_updated_value =
        entry_dict->FindKey(kTimeLimitLastUpdatedAt);

    int64_t last_updated;
    if (!base::StringToInt64(last_updated_value->GetString(), &last_updated)) {
      // Cannot process entry without a valid last updated.
      continue;
    }

    Weekday weekday = GetWeekday(weekday_key);
    TimeUsageLimitEntry entry;
    entry.usage_quota = base::TimeDelta::FromMinutes(usage_quota->GetInt());
    entry.last_updated = base::Time::UnixEpoch() +
                         base::TimeDelta::FromMilliseconds(last_updated);
    entries[weekday] = std::move(entry);
  }
}

TimeUsageLimit::~TimeUsageLimit() = default;

TimeUsageLimit::TimeUsageLimit(TimeUsageLimit&&) = default;

TimeUsageLimit& TimeUsageLimit::operator=(TimeUsageLimit&&) = default;

Override::Override(const base::Value& override_dict) {
  const base::Value* action_value = override_dict.FindKey(kOverrideAction);
  const base::Value* created_at_value =
      override_dict.FindKey(kOverrideActionCreatedAt);

  if (!action_value || !created_at_value)
    return;

  int64_t created_at_millis;
  if (!base::StringToInt64(created_at_value->GetString(), &created_at_millis)) {
    // Cannot process entry without a valid creation time.
    return;
  }

  action = action_value->GetString() == kOverrideActionLock ? Action::kLock
                                                            : Action::kUnlock;
  created_at = base::Time::UnixEpoch() +
               base::TimeDelta::FromMilliseconds(created_at_millis);

  const base::Value* duration_value = override_dict.FindPath(
      {kOverrideActionSpecificData, kOverrideActionDurationMins});
  if (duration_value)
    duration = base::TimeDelta::FromMinutes(duration_value->GetInt());
}

Override::~Override() = default;

Override::Override(Override&&) = default;

Override& Override::operator=(Override&&) = default;

Weekday GetWeekday(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return static_cast<Weekday>(exploded.day_of_week);
}

Weekday WeekdayShift(Weekday current_day, int shift) {
  return static_cast<Weekday>(static_cast<int>(current_day) +
                              shift % static_cast<int>(Weekday::kCount));
}

}  // namespace internal

namespace usage_time_limit {

State GetState(const std::unique_ptr<base::DictionaryValue>& time_limit,
               const base::TimeDelta& used_time,
               const base::Time& usage_timestamp,
               const base::Time& current_time,
               const base::Optional<State>& previous_state) {
  // TODO: Implement method.
  return State();
}

base::Time GetExpectedResetTime(
    const std::unique_ptr<base::DictionaryValue>& time_limit,
    base::Time current_time) {
  // TODO: Implement this method.
  return base::Time();
}

}  // namespace usage_time_limit
}  // namespace chromeos
