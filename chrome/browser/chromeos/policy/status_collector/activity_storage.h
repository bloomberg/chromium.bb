// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_ACTIVITY_STORAGE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_ACTIVITY_STORAGE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"

namespace base {
class DictionaryValue;
}

class PrefService;

namespace policy {

// Handles storing activity time periods needed for reporting. Provides
// filtering of the user identifying data.
//
// TODO(crbug.com/827386): refactor this class, removing child related parts.
class ActivityStorage {
 public:
  // Stored activity period.
  struct ActivityPeriod {
    // Email can be empty.
    std::string user_email;

    // Timestamp dating the beginning of the captured activity.
    int64_t start_timestamp;

    // User's activity in milliseconds.
    int activity_milliseconds;
  };

  // Creates activity storage. Activity data will be stored in the given
  // |pref_service| under |pref_name| preference. Activity data are aggregated
  // by day. The start of the new day is counted from |activity_day_start| that
  // represents the distance from midnight.
  ActivityStorage(PrefService* pref_service,
                  const std::string& pref_name,
                  base::TimeDelta activity_day_start,
                  bool is_enterprise_reporting);
  ~ActivityStorage();

  // Adds an activity period. Accepts empty |active_user_email| if it should not
  // be stored.
  void AddActivityPeriod(base::Time start,
                         base::Time end,
                         base::Time now,
                         const std::string& active_user_email);

  // Clears stored activity periods outside of storage range defined by
  // |max_past_activity_interval| and |max_future_activity_interval| from
  // |base_time|.
  void PruneActivityPeriods(base::Time base_time,
                            base::TimeDelta max_past_activity_interval,
                            base::TimeDelta max_future_activity_interval);

  // Trims the store activity periods to only retain data within the
  // [|min_day_key|, |max_day_key|). The record for |min_day_key| will be
  // adjusted by subtracting |min_day_trim_duration|.
  void TrimActivityPeriods(int64_t min_day_key,
                           int min_day_trim_duration,
                           int64_t max_day_key);

  // Updates stored activity period according to users' reporting preferences.
  // Removes user's email and aggregates the activity data if user's information
  // should no longer be reported.
  void FilterActivityPeriodsByUsers(
      const std::vector<std::string>& reporting_users);

  // Returns the list of stored activity periods. Aggregated data are returned
  // without email addresses if |omit_emails| is set.
  std::vector<ActivityPeriod> GetFilteredActivityPeriods(bool omit_emails);

 private:
  static std::string MakeActivityPeriodPrefKey(int64_t start,
                                               const std::string& user_email);
  static bool ParseActivityPeriodPrefKey(const std::string& key,
                                         int64_t* start_timestamp,
                                         std::string* user_email);
  void ProcessActivityPeriods(const base::DictionaryValue& activity_times,
                              const std::vector<std::string>& reporting_users,
                              base::DictionaryValue* const filtered_times);
  void StoreChildScreenTime(base::Time activity_day_start,
                            base::TimeDelta activity,
                            base::Time now);

  // Determine the day key (milliseconds since epoch for corresponding
  // |day_start_| in UTC) for a given |timestamp|.
  int64_t TimestampToDayKey(base::Time timestamp);

  PrefService* const pref_service_ = nullptr;
  const std::string pref_name_;

  // New day start time used for aggregating data represented by the distance
  // from midnight.
  const base::TimeDelta day_start_;

  // Whether reporting is for enterprise or consumer.
  bool is_enterprise_reporting_ = false;

  DISALLOW_COPY_AND_ASSIGN(ActivityStorage);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_ACTIVITY_STORAGE_H_
