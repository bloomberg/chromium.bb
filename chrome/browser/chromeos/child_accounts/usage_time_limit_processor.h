// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Processor for the UsageTimeLimit policy. Used to determine the current state
// of the client, for example if it is locked and the reason why it may be
// locked.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_USAGE_TIME_LIMIT_PROCESSOR_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_USAGE_TIME_LIMIT_PROCESSOR_H_

#include <memory>
#include <unordered_map>

#include "base/optional.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/settings/timezone_settings.h"

namespace chromeos {
namespace usage_time_limit {
namespace internal {

enum class Weekday {
  kSunday = 0,
  kMonday,
  kTuesday,
  kWednesday,
  kThursday,
  kFriday,
  kSaturday,
  kCount,
};

struct TimeWindowLimitBoundaries {
  base::Time starts;
  base::Time ends;
};

struct TimeWindowLimitEntry {
  TimeWindowLimitEntry();

  // Whether the time window limit entry ends on the following day from its
  // start.
  bool IsOvernight() const;

  // Returns a pair containing the timestamps for the start and end of a time
  // window limit. The input parameter is the UTC midnight on of the start day.
  TimeWindowLimitBoundaries GetLimits(base::Time start_day_midnight);

  // Start time of time window limit. This is the distance from midnight.
  base::TimeDelta starts_at;
  // End time of time window limit. This is the distance from midnight.
  base::TimeDelta ends_at;
  // Last time this entry was updated.
  base::Time last_updated;
};

class TimeWindowLimit {
 public:
  explicit TimeWindowLimit(const base::Value& window_limit_dict);
  ~TimeWindowLimit();
  TimeWindowLimit(TimeWindowLimit&&);
  TimeWindowLimit& operator=(TimeWindowLimit&&);

  std::unordered_map<Weekday, base::Optional<TimeWindowLimitEntry>> entries;

 private:
  DISALLOW_COPY_AND_ASSIGN(TimeWindowLimit);
};

struct TimeUsageLimitEntry {
  TimeUsageLimitEntry();

  base::TimeDelta usage_quota;
  base::Time last_updated;
};

class TimeUsageLimit {
 public:
  explicit TimeUsageLimit(const base::Value& usage_limit_dict);
  ~TimeUsageLimit();
  TimeUsageLimit(TimeUsageLimit&&);
  TimeUsageLimit& operator=(TimeUsageLimit&&);

  std::unordered_map<Weekday, base::Optional<TimeUsageLimitEntry>> entries;
  base::TimeDelta resets_at;

 private:
  DISALLOW_COPY_AND_ASSIGN(TimeUsageLimit);
};

class TimeLimitOverride {
 public:
  enum class Action { kLock, kUnlock };

  explicit TimeLimitOverride(const base::Value& override_dict);
  ~TimeLimitOverride();
  TimeLimitOverride(TimeLimitOverride&&);
  TimeLimitOverride& operator=(TimeLimitOverride&&);

  Action action;
  base::Time created_at;
  base::Optional<base::TimeDelta> duration;

 private:
  DISALLOW_COPY_AND_ASSIGN(TimeLimitOverride);
};

}  // namespace internal

enum class ActivePolicies {
  kNoActivePolicy,
  kOverride,
  kFixedLimit,  // Past bed time (ie, 9pm)
  kUsageLimit   // Too much time on screen (ie, 30 minutes per day)
};

struct State {
  // Whether the device is currently locked.
  bool is_locked = false;

  // Which policy is responsible for the current state.
  // If it is locked, one of [ kOverride, kFixedLimit, kUsageLimit ]
  // If it is not locked, one of [ kNoActivePolicy, kOverride ]
  ActivePolicies active_policy;

  // Whether time_usage_limit is currently active.
  bool is_time_usage_limit_enabled = false;

  // Remaining screen usage quota. Only available if
  // is_time_limit_enabled = true
  base::TimeDelta remaining_usage;

  // When the time usage limit started being enforced. Only available when
  // is_time_usage_limit_enabled = true and remaining_usage is 0, which means
  // that the time usage limit is enforced, and therefore should have a start
  // time.
  base::Time time_usage_limit_started;

  // Next epoch time that time limit state could change. This could be the
  // start time of the next fixed window limit, the end time of the current
  // fixed limit, the earliest time a usage limit could be reached, or the
  // next time when screen time will start.
  base::Time next_state_change_time;

  // The policy that will be active in the next state.
  ActivePolicies next_state_active_policy;

  // This is the next time that the user's session will be unlocked. This is
  // only set when is_locked=true;
  base::Time next_unlock_time;

  // Last time the state changed.
  base::Time last_state_changed;
};

// Returns the current state of the user session with the given usage time limit
// policy.
State GetState(const std::unique_ptr<base::DictionaryValue>& time_limit,
               const base::TimeDelta& used_time,
               const base::Time& usage_timestamp,
               const base::Time& current_time,
               const icu::TimeZone* const time_zone,
               const base::Optional<State>& previous_state);

// Returns the expected time that the used time stored should be reset.
base::Time GetExpectedResetTime(
    const std::unique_ptr<base::DictionaryValue>& time_limit,
    base::Time current_time,
    const icu::TimeZone* const time_zone);

}  // namespace usage_time_limit
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_USAGE_TIME_LIMIT_PROCESSOR_H_
