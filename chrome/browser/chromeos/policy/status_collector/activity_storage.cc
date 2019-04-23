// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_collector/activity_storage.h"

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"

namespace policy {
namespace {

using ::base::Time;
using ::base::TimeDelta;

// Activity periods are keyed with day and user in format:
// '<day_timestamp>:<BASE64 encoded user email>'
constexpr char kActivityKeySeparator = ':';

}  // namespace

ActivityStorage::ActivityStorage(PrefService* pref_service,
                                 const std::string& pref_name,
                                 TimeDelta activity_day_start,
                                 bool is_enterprise_reporting)
    : pref_service_(pref_service),
      pref_name_(pref_name),
      day_start_(activity_day_start),
      is_enterprise_reporting_(is_enterprise_reporting) {
  DCHECK(pref_service_);
  const PrefService::PrefInitializationStatus pref_service_status =
      pref_service_->GetInitializationStatus();
  DCHECK(pref_service_status != PrefService::INITIALIZATION_STATUS_WAITING &&
         pref_service_status != PrefService::INITIALIZATION_STATUS_ERROR);
}

ActivityStorage::~ActivityStorage() = default;

void ActivityStorage::AddActivityPeriod(Time start,
                                        Time end,
                                        Time now,
                                        const std::string& active_user_email) {
  DCHECK(start <= end);

  // Maintain the list of active periods in a local_state pref.
  DictionaryPrefUpdate update(pref_service_, pref_name_);
  base::DictionaryValue* activity_times = update.Get();

  // Assign the period to day buckets in local time.
  Time day_start = start.LocalMidnight() + day_start_;
  if (start < day_start)
    day_start -= TimeDelta::FromDays(1);
  while (day_start < end) {
    day_start += TimeDelta::FromDays(1);
    int64_t activity = (std::min(end, day_start) - start).InMilliseconds();
    const std::string key =
        MakeActivityPeriodPrefKey(TimestampToDayKey(start), active_user_email);
    int previous_activity = 0;
    activity_times->GetInteger(key, &previous_activity);
    activity_times->SetInteger(key, previous_activity + activity);

    // If the user is a child, the child screen time pref may need to be
    // updated.
    if (user_manager::UserManager::Get()->IsLoggedInAsChildUser() &&
        !is_enterprise_reporting_) {
      StoreChildScreenTime(day_start - TimeDelta::FromDays(1),
                           TimeDelta::FromMilliseconds(activity), now);
    }

    start = day_start;
  }
}

void ActivityStorage::PruneActivityPeriods(
    Time base_time,
    TimeDelta max_past_activity_interval,
    TimeDelta max_future_activity_interval) {
  Time min_time = base_time - max_past_activity_interval;
  Time max_time = base_time + max_future_activity_interval;
  TrimActivityPeriods(TimestampToDayKey(min_time), 0,
                      TimestampToDayKey(max_time));
}

void ActivityStorage::TrimActivityPeriods(int64_t min_day_key,
                                          int min_day_trim_duration,
                                          int64_t max_day_key) {
  const base::DictionaryValue* activity_times =
      pref_service_->GetDictionary(pref_name_);

  std::unique_ptr<base::DictionaryValue> copy(activity_times->DeepCopy());
  for (base::DictionaryValue::Iterator it(*activity_times); !it.IsAtEnd();
       it.Advance()) {
    int64_t timestamp;
    std::string active_user_email;
    if (ParseActivityPeriodPrefKey(it.key(), &timestamp, &active_user_email)) {
      // Remove data that is too old, or too far in the future.
      if (timestamp >= min_day_key && timestamp < max_day_key) {
        if (timestamp == min_day_key) {
          int new_activity_duration = 0;
          if (it.value().GetAsInteger(&new_activity_duration)) {
            new_activity_duration =
                std::max(new_activity_duration - min_day_trim_duration, 0);
          }
          copy->SetInteger(it.key(), new_activity_duration);
        }
        continue;
      }
    }
    // The entry is out of range or couldn't be parsed. Remove it.
    copy->Remove(it.key(), NULL);
  }
  pref_service_->Set(pref_name_, *copy);
}

void ActivityStorage::FilterActivityPeriodsByUsers(
    const std::vector<std::string>& reporting_users) {
  const base::DictionaryValue* stored_activity_periods =
      pref_service_->GetDictionary(pref_name_);
  base::DictionaryValue filtered_activity_periods;
  ProcessActivityPeriods(*stored_activity_periods, reporting_users,
                         &filtered_activity_periods);
  pref_service_->Set(pref_name_, filtered_activity_periods);
}

std::vector<ActivityStorage::ActivityPeriod>
ActivityStorage::GetFilteredActivityPeriods(bool omit_emails) {
  DictionaryPrefUpdate update(pref_service_, pref_name_);
  base::DictionaryValue* stored_activity_periods = update.Get();

  base::DictionaryValue filtered_activity_periods;
  if (omit_emails) {
    std::vector<std::string> empty_user_list;
    ProcessActivityPeriods(*stored_activity_periods, empty_user_list,
                           &filtered_activity_periods);
    stored_activity_periods = &filtered_activity_periods;
  }

  std::vector<ActivityPeriod> activity_periods;
  for (base::DictionaryValue::Iterator it(*stored_activity_periods);
       !it.IsAtEnd(); it.Advance()) {
    ActivityPeriod activity_period;
    if (ParseActivityPeriodPrefKey(it.key(), &activity_period.start_timestamp,
                                   &activity_period.user_email) &&
        it.value().GetAsInteger(&activity_period.activity_milliseconds)) {
      activity_periods.push_back(activity_period);
    }
  }
  return activity_periods;
}

// static
std::string ActivityStorage::MakeActivityPeriodPrefKey(
    int64_t start,
    const std::string& user_email) {
  const std::string day_key = base::NumberToString(start);
  if (user_email.empty())
    return day_key;

  std::string encoded_email;
  base::Base64Encode(user_email, &encoded_email);
  return day_key + kActivityKeySeparator + encoded_email;
}

// static
bool ActivityStorage::ParseActivityPeriodPrefKey(const std::string& key,
                                                 int64_t* start_timestamp,
                                                 std::string* user_email) {
  auto separator_pos = key.find(kActivityKeySeparator);
  if (separator_pos == std::string::npos) {
    user_email->clear();
    return base::StringToInt64(key, start_timestamp);
  }
  return base::StringToInt64(key.substr(0, separator_pos), start_timestamp) &&
         base::Base64Decode(key.substr(separator_pos + 1), user_email);
}

void ActivityStorage::ProcessActivityPeriods(
    const base::DictionaryValue& activity_times,
    const std::vector<std::string>& reporting_users,
    base::DictionaryValue* const filtered_times) {
  std::set<std::string> reporting_users_set(reporting_users.begin(),
                                            reporting_users.end());
  const std::string empty;
  for (const auto& it : activity_times.DictItems()) {
    DCHECK(it.second.is_int());
    int64_t timestamp;
    std::string user_email;
    if (!ParseActivityPeriodPrefKey(it.first, &timestamp, &user_email))
      continue;
    if (!user_email.empty() && reporting_users_set.count(user_email) == 0) {
      int value = 0;
      std::string timestamp_str = MakeActivityPeriodPrefKey(timestamp, empty);
      const base::Value* prev_value = filtered_times->FindKeyOfType(
          timestamp_str, base::Value::Type::INTEGER);
      if (prev_value)
        value = prev_value->GetInt();
      filtered_times->SetKey(timestamp_str,
                             base::Value(value + it.second.GetInt()));
    } else {
      filtered_times->SetKey(it.first, it.second.Clone());
    }
  }
}

int64_t ActivityStorage::TimestampToDayKey(Time timestamp) {
  Time::Exploded exploded;
  Time day_start = timestamp.LocalMidnight() + day_start_;
  if (timestamp < day_start)
    day_start -= TimeDelta::FromDays(1);
  day_start.LocalExplode(&exploded);
  Time out_time;
  bool conversion_success = Time::FromUTCExploded(exploded, &out_time);
  DCHECK(conversion_success);
  return out_time.ToJavaTime();
}

void ActivityStorage::StoreChildScreenTime(Time activity_day_start,
                                           TimeDelta activity,
                                           Time now) {
  DCHECK(user_manager::UserManager::Get()->IsLoggedInAsChildUser() &&
         !is_enterprise_reporting_);

  // Today's start time.
  Time today_start = now.LocalMidnight() + day_start_;
  if (today_start > now)
    today_start -= TimeDelta::FromDays(1);

  // The activity windows always start and end on the reset time of two
  // consecutive days, so it is not possible to have a window starting after
  // the current day's reset time.
  DCHECK(activity_day_start <= today_start);

  TimeDelta previous_activity = TimeDelta::FromMilliseconds(
      pref_service_->GetInteger(prefs::kChildScreenTimeMilliseconds));

  // If this activity window belongs to the current day, the screen time pref
  // should be updated.
  if (activity_day_start == today_start) {
    pref_service_->SetInteger(prefs::kChildScreenTimeMilliseconds,
                              (previous_activity + activity).InMilliseconds());
    pref_service_->SetTime(prefs::kLastChildScreenTimeSaved, now);
  }
  pref_service_->CommitPendingWrite();
}

}  // namespace policy
