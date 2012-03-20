// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_status_collector.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"

using base::Time;
using base::TimeDelta;
using chromeos::VersionLoader;

namespace em = enterprise_management;

namespace {
// How many seconds of inactivity triggers the idle state.
const unsigned int kIdleStateThresholdSeconds = 300;

// How many days in the past to store active periods for.
const unsigned int kMaxStoredPastActivityDays = 30;

// How many days in the future to store active periods for.
const unsigned int kMaxStoredFutureActivityDays = 2;

const int64 kMillisecondsPerDay = Time::kMicrosecondsPerDay / 1000;

// Record device activity for the specified day into the given dictionary.
void AddDeviceActivity(DictionaryValue* activity_times,
                       Time day_midnight,
                       TimeDelta activity) {
  DCHECK(activity.InMilliseconds() < kMillisecondsPerDay);
  int64 day_start_timestamp =
      (day_midnight - Time::UnixEpoch()).InMilliseconds();
  std::string day_key = base::Int64ToString(day_start_timestamp);
  int previous_activity = 0;
  activity_times->GetInteger(day_key, &previous_activity);
  activity_times->SetInteger(day_key,
                             previous_activity + activity.InMilliseconds());
}

}  // namespace

namespace policy {

DeviceStatusCollector::DeviceStatusCollector(
    PrefService* local_state,
    chromeos::system::StatisticsProvider* provider)
    : max_stored_past_activity_days_(kMaxStoredPastActivityDays),
      max_stored_future_activity_days_(kMaxStoredFutureActivityDays),
      local_state_(local_state),
      last_idle_check_(Time()),
      statistics_provider_(provider),
      report_version_info_(false),
      report_activity_times_(false),
      report_boot_mode_(false) {
  timer_.Start(FROM_HERE,
               TimeDelta::FromSeconds(kPollIntervalSeconds),
               this, &DeviceStatusCollector::CheckIdleState);

  cros_settings_ = chromeos::CrosSettings::Get();

  // Watch for changes to the individual policies that control what the status
  // reports contain.
  cros_settings_->AddSettingsObserver(chromeos::kReportDeviceVersionInfo, this);
  cros_settings_->AddSettingsObserver(chromeos::kReportDeviceActivityTimes,
                                      this);
  cros_settings_->AddSettingsObserver(chromeos::kReportDeviceBootMode, this);

  // Fetch the current values of the policies.
  UpdateReportingSettings();

  // Get the the OS and firmware version info.
  version_loader_.GetVersion(&consumer_,
                             base::Bind(&DeviceStatusCollector::OnOSVersion,
                                        base::Unretained(this)),
                             VersionLoader::VERSION_FULL);
  version_loader_.GetFirmware(&consumer_,
                              base::Bind(&DeviceStatusCollector::OnOSFirmware,
                                         base::Unretained(this)));
}

DeviceStatusCollector::~DeviceStatusCollector() {
  cros_settings_->RemoveSettingsObserver(chromeos::kReportDeviceVersionInfo,
                                         this);
  cros_settings_->RemoveSettingsObserver(chromeos::kReportDeviceActivityTimes,
                                         this);
  cros_settings_->RemoveSettingsObserver(chromeos::kReportDeviceBootMode, this);
}

// static
void DeviceStatusCollector::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterDictionaryPref(prefs::kDeviceActivityTimes,
                                      new DictionaryValue);
}

void DeviceStatusCollector::CheckIdleState() {
  CalculateIdleState(kIdleStateThresholdSeconds,
      base::Bind(&DeviceStatusCollector::IdleStateCallback,
                 base::Unretained(this)));
}

void DeviceStatusCollector::UpdateReportingSettings() {
  // Attempt to fetch the current value of the reporting settings.
  // If trusted values are not available, register this function to be called
  // back when they are available.
  if (!cros_settings_->PrepareTrustedValues(
      base::Bind(&DeviceStatusCollector::UpdateReportingSettings,
                 base::Unretained(this)))) {
    return;
  }
  cros_settings_->GetBoolean(
      chromeos::kReportDeviceVersionInfo, &report_version_info_);
  cros_settings_->GetBoolean(
      chromeos::kReportDeviceActivityTimes, &report_activity_times_);
  cros_settings_->GetBoolean(
      chromeos::kReportDeviceBootMode, &report_boot_mode_);
}

Time DeviceStatusCollector::GetCurrentTime() {
  return Time::Now();
}

// Remove all out-of-range activity times from the local store.
void DeviceStatusCollector::PruneStoredActivityPeriods(Time base_time) {
  const DictionaryValue* activity_times =
      local_state_->GetDictionary(prefs::kDeviceActivityTimes);
  if (activity_times->size() <=
      max_stored_past_activity_days_ + max_stored_future_activity_days_)
    return;

  Time min_time =
      base_time - TimeDelta::FromDays(max_stored_past_activity_days_);
  Time max_time =
      base_time + TimeDelta::FromDays(max_stored_future_activity_days_);
  const Time epoch = Time::UnixEpoch();

  scoped_ptr<DictionaryValue> copy(activity_times->DeepCopy());
  for (DictionaryValue::key_iterator it = activity_times->begin_keys();
       it != activity_times->end_keys(); ++it) {
    int64 timestamp;

    if (base::StringToInt64(*it, &timestamp)) {
      // Remove data that is too old, or too far in the future.
      Time day_midnight = epoch + TimeDelta::FromMilliseconds(timestamp);
      if (day_midnight > min_time && day_midnight < max_time)
        continue;
    }
    // The entry is out of range or couldn't be parsed. Remove it.
    copy->Remove(*it, NULL);
  }
  local_state_->Set(prefs::kDeviceActivityTimes, *copy);
}

void DeviceStatusCollector::AddActivePeriod(Time start, Time end) {
  DCHECK(start < end);

  // Maintain the list of active periods in a local_state pref.
  DictionaryPrefUpdate update(local_state_, prefs::kDeviceActivityTimes);
  DictionaryValue* activity_times = update.Get();

  Time midnight = end.LocalMidnight();

  // Figure out UTC midnight on the same day as the local day.
  Time::Exploded exploded;
  midnight.LocalExplode(&exploded);
  Time utc_midnight = Time::FromUTCExploded(exploded);

  // Record the device activity for today.
  TimeDelta activity_today = end - MAX(midnight, start);
  AddDeviceActivity(activity_times, utc_midnight, activity_today);

  // If this interval spans two days, record activity for yesterday too.
  if (start < midnight) {
    AddDeviceActivity(activity_times,
                      utc_midnight - TimeDelta::FromDays(1),
                      midnight - start);
  }
}

void DeviceStatusCollector::IdleStateCallback(IdleState state) {
  // Do nothing if device activity reporting is disabled.
  if (!report_activity_times_)
    return;

  Time now = GetCurrentTime();

  if (state == IDLE_STATE_ACTIVE) {
    // If it's been too long since the last report, or if the activity is
    // negative (which can happen when the clock changes), assume a single
    // interval of activity.
    int active_seconds = (now - last_idle_check_).InSeconds();
    if (active_seconds < 0 ||
        active_seconds >= static_cast<int>((2 * kPollIntervalSeconds)))
      AddActivePeriod(now - TimeDelta::FromSeconds(kPollIntervalSeconds), now);
    else
      AddActivePeriod(last_idle_check_, now);

    PruneStoredActivityPeriods(now);
  }
  last_idle_check_ = now;
}

void DeviceStatusCollector::GetActivityTimes(
    em::DeviceStatusReportRequest* request) {
  DictionaryPrefUpdate update(local_state_, prefs::kDeviceActivityTimes);
  DictionaryValue* activity_times = update.Get();

  for (DictionaryValue::key_iterator it = activity_times->begin_keys();
       it != activity_times->end_keys(); ++it) {
    int64 start_timestamp;
    int activity_milliseconds;
    if (base::StringToInt64(*it, &start_timestamp) &&
        activity_times->GetInteger(*it, &activity_milliseconds)) {
      // This is correct even when there are leap seconds, because when a leap
      // second occurs, two consecutive seconds have the same timestamp.
      int64 end_timestamp = start_timestamp + kMillisecondsPerDay;

      em::ActiveTimePeriod* active_period = request->add_active_period();
      em::TimePeriod* period = active_period->mutable_time_period();
      period->set_start_timestamp(start_timestamp);
      period->set_end_timestamp(end_timestamp);
      active_period->set_active_duration(activity_milliseconds);
    } else {
      NOTREACHED();
    }
  }
  activity_times->Clear();
}

void DeviceStatusCollector::GetVersionInfo(
    em::DeviceStatusReportRequest* request) {
  chrome::VersionInfo version_info;
  request->set_browser_version(version_info.Version());
  request->set_os_version(os_version_);
  request->set_firmware_version(firmware_version_);
}

void DeviceStatusCollector::GetBootMode(
    em::DeviceStatusReportRequest* request) {
  std::string dev_switch_mode;
  if (statistics_provider_->GetMachineStatistic(
      "devsw_boot", &dev_switch_mode)) {
    if (dev_switch_mode == "1")
      request->set_boot_mode("Dev");
    else if (dev_switch_mode == "0")
      request->set_boot_mode("Verified");
  }
}

void DeviceStatusCollector::GetStatus(em::DeviceStatusReportRequest* request) {
  if (report_activity_times_)
    GetActivityTimes(request);

  if (report_version_info_)
    GetVersionInfo(request);

  if (report_boot_mode_)
    GetBootMode(request);
}

void DeviceStatusCollector::OnOSVersion(VersionLoader::Handle handle,
                                        std::string version) {
  os_version_ = version;
}

void DeviceStatusCollector::OnOSFirmware(VersionLoader::Handle handle,
                                         std::string version) {
  firmware_version_ = version;
}

void DeviceStatusCollector::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_SYSTEM_SETTING_CHANGED)
    UpdateReportingSettings();
  else
    NOTREACHED();
}

}  // namespace policy
