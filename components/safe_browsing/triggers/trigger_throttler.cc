// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/trigger_throttler.h"

#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/safe_browsing/features.h"

namespace safe_browsing {
const char kTriggerTypeAndQuotaParam[] = "trigger_type_and_quota_csv";

namespace {
const size_t kUnlimitedTriggerQuota = std::numeric_limits<size_t>::max();
constexpr base::TimeDelta kOneDayTimeDelta = base::TimeDelta::FromDays(1);

// Predicate used to search |trigger_type_and_quota_list_| by trigger type.
class TriggerTypeIs {
 public:
  explicit TriggerTypeIs(const TriggerType type) : type_(type) {}
  bool operator()(const TriggerTypeAndQuotaItem& trigger_type_and_quota) {
    return type_ == trigger_type_and_quota.first;
  }

 private:
  TriggerType type_;
};

void ParseTriggerTypeAndQuotaParam(
    std::vector<TriggerTypeAndQuotaItem>* trigger_type_and_quota_list) {
  DCHECK(trigger_type_and_quota_list);
  // If the feature is disabled we just use the default list. Otherwise the list
  // from the Finch param will be the one used.
  if (!base::FeatureList::IsEnabled(kTriggerThrottlerDailyQuotaFeature)) {
    return;
  }

  trigger_type_and_quota_list->clear();
  const std::string& trigger_and_quota_csv_param =
      base::GetFieldTrialParamValueByFeature(kTriggerThrottlerDailyQuotaFeature,
                                             kTriggerTypeAndQuotaParam);
  if (trigger_and_quota_csv_param.empty()) {
    return;
  }

  std::vector<std::string> split =
      base::SplitString(trigger_and_quota_csv_param, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  // If we don't have the right number of pairs in the csv then don't bother
  // parsing further.
  if (split.size() % 2 != 0) {
    return;
  }
  for (size_t i = 0; i < split.size(); i += 2) {
    // Make sure both the trigger type and quota are integers. Skip them if not.
    int trigger_type_int = -1;
    int quota_int = -1;
    if (!base::StringToInt(split[i], &trigger_type_int) ||
        !base::StringToInt(split[i + 1], &quota_int)) {
      continue;
    }
    trigger_type_and_quota_list->push_back(
        std::make_pair(static_cast<TriggerType>(trigger_type_int), quota_int));
  }

  std::sort(trigger_type_and_quota_list->begin(),
            trigger_type_and_quota_list->end(),
            [](const TriggerTypeAndQuotaItem& a,
               const TriggerTypeAndQuotaItem& b) { return a.first < b.first; });
}

size_t GetDailyQuotaForTrigger(
    const TriggerType trigger_type,
    const std::vector<TriggerTypeAndQuotaItem>& trigger_quota_list) {
  switch (trigger_type) {
    case TriggerType::SECURITY_INTERSTITIAL:
    case TriggerType::GAIA_PASSWORD_REUSE:
      return kUnlimitedTriggerQuota;
    case TriggerType::AD_SAMPLE:
      // These triggers have quota configured via Finch, lookup the value in
      // |trigger_quota_list|.
      const auto& trigger_quota_iter =
          std::find_if(trigger_quota_list.begin(), trigger_quota_list.end(),
                       TriggerTypeIs(trigger_type));
      if (trigger_quota_iter != trigger_quota_list.end())
        return trigger_quota_iter->second;

      break;
  }
  // By default, unhandled or unconfigured trigger types have no quota.
  return 0;
}

}  // namespace

TriggerThrottler::TriggerThrottler() : clock_(new base::DefaultClock()) {
  ParseTriggerTypeAndQuotaParam(&trigger_type_and_quota_list_);
}

TriggerThrottler::~TriggerThrottler() {}

void TriggerThrottler::SetClockForTesting(
    std::unique_ptr<base::Clock> test_clock) {
  clock_ = std::move(test_clock);
}

bool TriggerThrottler::TriggerCanFire(const TriggerType trigger_type) const {
  // Lookup how many times this trigger is allowed to fire each day.
  const size_t trigger_quota =
      GetDailyQuotaForTrigger(trigger_type, trigger_type_and_quota_list_);

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

  // Otherwise, we have more events than quota, check which day they occurred
  // on. Newest events are at the end of vector so we can simply look at the
  // Nth-from-last entry (where N is the quota) to see if it happened within
  // the current day or earlier.
  base::Time min_timestamp = clock_->Now() - kOneDayTimeDelta;
  const size_t pos = timestamps.size() - trigger_quota;
  return timestamps.at(pos) < min_timestamp.ToTimeT();
}

void TriggerThrottler::TriggerFired(const TriggerType trigger_type) {
  // Lookup how many times this trigger is allowed to fire each day.
  const size_t trigger_quota =
      GetDailyQuotaForTrigger(trigger_type, trigger_type_and_quota_list_);

  // For triggers that always fire, don't bother tracking quota.
  if (trigger_quota == kUnlimitedTriggerQuota)
    return;

  // Otherwise, record that the trigger fired.
  std::vector<time_t>* timestamps = &trigger_events_[trigger_type];
  timestamps->push_back(clock_->Now().ToTimeT());

  // Clean up the trigger events map.
  CleanupOldEvents();
}

void TriggerThrottler::CleanupOldEvents() {
  for (const auto& map_iter : trigger_events_) {
    const TriggerType trigger_type = map_iter.first;
    const size_t trigger_quota =
        GetDailyQuotaForTrigger(trigger_type, trigger_type_and_quota_list_);
    const std::vector<time_t>& trigger_times = map_iter.second;

    // Skip the cleanup if we have quota room, quotas should generally be small.
    if (trigger_times.size() < trigger_quota)
      return;

    std::vector<time_t> tmp_trigger_times;
    base::Time min_timestamp = clock_->Now() - kOneDayTimeDelta;
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
