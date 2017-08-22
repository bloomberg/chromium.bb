// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/trigger_throttler.h"

#include "base/stl_util.h"
#include "base/time/time.h"

namespace safe_browsing {

namespace {
const size_t kUnlimitedTriggerQuota = std::numeric_limits<size_t>::max();
constexpr base::TimeDelta kOneDayTimeDelta = base::TimeDelta::FromDays(1);

size_t GetDailyQuotaForTrigger(const TriggerType trigger_type) {
  switch (trigger_type) {
    case TriggerType::SECURITY_INTERSTITIAL:
      return kUnlimitedTriggerQuota;
    case TriggerType::AD_SAMPLE:
      // TODO(lpz): Introduce a finch param to configure quota per-trigger-type.
      return 0;
  }
  // By default, unhandled trigger types have no quota.
  return 0;
}

}  // namespace

TriggerThrottler::TriggerThrottler() {}

TriggerThrottler::~TriggerThrottler() {}

bool TriggerThrottler::TriggerCanFire(const TriggerType trigger_type) const {
  // Lookup how many times this trigger is allowed to fire each day.
  const size_t trigger_quota = GetDailyQuotaForTrigger(trigger_type);

  // Some basic corner cases for triggers that always fire, or disabled
  // triggers that never fire.
  if (trigger_quota == kUnlimitedTriggerQuota)
    return true;
  if (trigger_quota == 0)
    return false;

  // Other triggers are capped, see how many times this trigger has already
  // fired.
  if (!base::ContainsKey(trigger_events_, trigger_type))
    return true;

  const std::vector<time_t>& timestamps = trigger_events_.at(trigger_type);
  // More quota is available, so the trigger can fire again.
  if (trigger_quota > timestamps.size())
    return true;

  // Otherwise, we have more events than quota, check which day they occured on.
  // Newest events are at the end of vector so we can simply look at the
  // Nth-from-last entry (where N is the quota) to see if it happened within
  // the current day or earlier.
  base::Time min_timestamp = base::Time::Now() - kOneDayTimeDelta;
  const size_t pos = timestamps.size() - trigger_quota + 1;
  return timestamps[pos] < min_timestamp.ToTimeT();
}

void TriggerThrottler::TriggerFired(const TriggerType trigger_type) {
  // Lookup how many times this trigger is allowed to fire each day.
  const size_t trigger_quota = GetDailyQuotaForTrigger(trigger_type);

  // For triggers that always fire, don't bother tracking quota.
  if (trigger_quota == kUnlimitedTriggerQuota)
    return;

  // Otherwise, record that the trigger fired.
  std::vector<time_t>* timestamps = &trigger_events_[trigger_type];
  timestamps->push_back(base::Time::Now().ToTimeT());

  // Clean up the trigger events map.
  CleanupOldEvents();
}

void TriggerThrottler::CleanupOldEvents() {
  for (const auto& map_iter : trigger_events_) {
    const TriggerType trigger_type = map_iter.first;
    const size_t trigger_quota = GetDailyQuotaForTrigger(trigger_type);
    const std::vector<time_t>& trigger_times = map_iter.second;

    // Skip the cleanup if we have quota room, quotas should generally be small.
    if (trigger_times.size() < trigger_quota)
      return;

    std::vector<time_t> tmp_trigger_times;
    base::Time min_timestamp = base::Time::Now() - kOneDayTimeDelta;
    // Go over the event times for this trigger and keep timestamps which are
    // newer than |min_timestamp|. We put timestamps in a temp vector that will
    // get swapped into the map in place of the existing vector.
    for (const time_t timestamp : trigger_times) {
      if (timestamp > min_timestamp.ToTimeT())
        tmp_trigger_times.push_back(timestamp);
    }

    trigger_events_[trigger_type].swap(tmp_trigger_times);
  }
}

}  // namespace safe_browsing
