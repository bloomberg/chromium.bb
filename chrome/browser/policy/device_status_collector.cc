// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_status_collector.h"

#include <limits>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

using base::Time;
using base::TimeDelta;
using chromeos::VersionLoader;

namespace em = enterprise_management;

namespace {
// How many seconds of inactivity triggers the idle state.
const int kIdleStateThresholdSeconds = 300;

// How many days in the past to store active periods for.
const unsigned int kMaxStoredPastActivityDays = 30;

// How many days in the future to store active periods for.
const unsigned int kMaxStoredFutureActivityDays = 2;

// How often, in seconds, to update the device location.
const unsigned int kGeolocationPollIntervalSeconds = 30 * 60;

const int64 kMillisecondsPerDay = Time::kMicrosecondsPerDay / 1000;

const char kLatitude[] = "latitude";

const char kLongitude[] = "longitude";

const char kAltitude[] = "altitude";

const char kAccuracy[] = "accuracy";

const char kAltitudeAccuracy[] = "altitude_accuracy";

const char kHeading[] = "heading";

const char kSpeed[] = "speed";

const char kTimestamp[] = "timestamp";

// Determine the day key (milliseconds since epoch for corresponding day in UTC)
// for a given |timestamp|.
int64 TimestampToDayKey(Time timestamp) {
  Time::Exploded exploded;
  timestamp.LocalMidnight().LocalExplode(&exploded);
  return (Time::FromUTCExploded(exploded) - Time::UnixEpoch()).InMilliseconds();
}

}  // namespace

namespace policy {

DeviceStatusCollector::DeviceStatusCollector(
    PrefService* local_state,
    chromeos::system::StatisticsProvider* provider,
    LocationUpdateRequester location_update_requester)
    : max_stored_past_activity_days_(kMaxStoredPastActivityDays),
      max_stored_future_activity_days_(kMaxStoredFutureActivityDays),
      local_state_(local_state),
      last_idle_check_(Time()),
      last_reported_day_(0),
      duration_for_last_reported_day_(0),
      geolocation_update_in_progress_(false),
      statistics_provider_(provider),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      location_update_requester_(location_update_requester),
      report_version_info_(false),
      report_activity_times_(false),
      report_boot_mode_(false),
      report_location_(false) {
  if (!location_update_requester_)
    location_update_requester_ = &content::RequestLocationUpdate;
  idle_poll_timer_.Start(FROM_HERE,
                         TimeDelta::FromSeconds(kIdlePollIntervalSeconds),
                         this, &DeviceStatusCollector::CheckIdleState);

  cros_settings_ = chromeos::CrosSettings::Get();

  // Watch for changes to the individual policies that control what the status
  // reports contain.
  cros_settings_->AddSettingsObserver(chromeos::kReportDeviceVersionInfo, this);
  cros_settings_->AddSettingsObserver(chromeos::kReportDeviceActivityTimes,
                                      this);
  cros_settings_->AddSettingsObserver(chromeos::kReportDeviceBootMode, this);
  cros_settings_->AddSettingsObserver(chromeos::kReportDeviceLocation, this);

  // The last known location is persisted in local state. This makes location
  // information available immediately upon startup and avoids the need to
  // reacquire the location on every user session change or browser crash.
  content::Geoposition position;
  std::string timestamp_str;
  int64 timestamp;
  const base::DictionaryValue* location =
      local_state_->GetDictionary(prefs::kDeviceLocation);
  if (location->GetDouble(kLatitude, &position.latitude) &&
      location->GetDouble(kLongitude, &position.longitude) &&
      location->GetDouble(kAltitude, &position.altitude) &&
      location->GetDouble(kAccuracy, &position.accuracy) &&
      location->GetDouble(kAltitudeAccuracy, &position.altitude_accuracy) &&
      location->GetDouble(kHeading, &position.heading) &&
      location->GetDouble(kSpeed, &position.speed) &&
      location->GetString(kTimestamp, &timestamp_str) &&
      base::StringToInt64(timestamp_str, &timestamp)) {
    position.timestamp = Time::FromInternalValue(timestamp);
    position_ = position;
  }

  // Fetch the current values of the policies.
  UpdateReportingSettings();

  // Get the the OS and firmware version info.
  version_loader_.GetVersion(
      VersionLoader::VERSION_FULL,
      base::Bind(&DeviceStatusCollector::OnOSVersion, base::Unretained(this)),
      &tracker_);
  version_loader_.GetFirmware(
      base::Bind(&DeviceStatusCollector::OnOSFirmware, base::Unretained(this)),
      &tracker_);
}

DeviceStatusCollector::~DeviceStatusCollector() {
  cros_settings_->RemoveSettingsObserver(chromeos::kReportDeviceVersionInfo,
                                         this);
  cros_settings_->RemoveSettingsObserver(chromeos::kReportDeviceActivityTimes,
                                         this);
  cros_settings_->RemoveSettingsObserver(chromeos::kReportDeviceBootMode, this);
  cros_settings_->RemoveSettingsObserver(chromeos::kReportDeviceLocation, this);
}

// static
void DeviceStatusCollector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kDeviceActivityTimes,
                                   new base::DictionaryValue);
  registry->RegisterDictionaryPref(prefs::kDeviceLocation,
                                   new base::DictionaryValue);
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
  if (chromeos::CrosSettingsProvider::TRUSTED !=
      cros_settings_->PrepareTrustedValues(
      base::Bind(&DeviceStatusCollector::UpdateReportingSettings,
                 weak_factory_.GetWeakPtr()))) {
    return;
  }
  cros_settings_->GetBoolean(
      chromeos::kReportDeviceVersionInfo, &report_version_info_);
  cros_settings_->GetBoolean(
      chromeos::kReportDeviceActivityTimes, &report_activity_times_);
  cros_settings_->GetBoolean(
      chromeos::kReportDeviceBootMode, &report_boot_mode_);
  cros_settings_->GetBoolean(
      chromeos::kReportDeviceLocation, &report_location_);

  if (report_location_) {
    ScheduleGeolocationUpdateRequest();
  } else {
    geolocation_update_timer_.Stop();
    position_ = content::Geoposition();
    local_state_->ClearPref(prefs::kDeviceLocation);
  }
}

Time DeviceStatusCollector::GetCurrentTime() {
  return Time::Now();
}

// Remove all out-of-range activity times from the local store.
void DeviceStatusCollector::PruneStoredActivityPeriods(Time base_time) {
  Time min_time =
      base_time - TimeDelta::FromDays(max_stored_past_activity_days_);
  Time max_time =
      base_time + TimeDelta::FromDays(max_stored_future_activity_days_);
  TrimStoredActivityPeriods(TimestampToDayKey(min_time), 0,
                            TimestampToDayKey(max_time));
}

void DeviceStatusCollector::TrimStoredActivityPeriods(int64 min_day_key,
                                                      int min_day_trim_duration,
                                                      int64 max_day_key) {
  const base::DictionaryValue* activity_times =
      local_state_->GetDictionary(prefs::kDeviceActivityTimes);

  scoped_ptr<base::DictionaryValue> copy(activity_times->DeepCopy());
  for (base::DictionaryValue::Iterator it(*activity_times); it.HasNext();
       it.Advance()) {
    int64 timestamp;
    if (base::StringToInt64(it.key(), &timestamp)) {
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
  local_state_->Set(prefs::kDeviceActivityTimes, *copy);
}

void DeviceStatusCollector::AddActivePeriod(Time start, Time end) {
  DCHECK(start < end);

  // Maintain the list of active periods in a local_state pref.
  DictionaryPrefUpdate update(local_state_, prefs::kDeviceActivityTimes);
  base::DictionaryValue* activity_times = update.Get();

  // Assign the period to day buckets in local time.
  Time midnight = start.LocalMidnight();
  while (midnight < end) {
    midnight += TimeDelta::FromDays(1);
    int64 activity = (std::min(end, midnight) - start).InMilliseconds();
    std::string day_key = base::Int64ToString(TimestampToDayKey(start));
    int previous_activity = 0;
    activity_times->GetInteger(day_key, &previous_activity);
    activity_times->SetInteger(day_key, previous_activity + activity);
    start = midnight;
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
        active_seconds >= static_cast<int>((2 * kIdlePollIntervalSeconds))) {
      AddActivePeriod(now - TimeDelta::FromSeconds(kIdlePollIntervalSeconds),
                      now);
    } else {
      AddActivePeriod(last_idle_check_, now);
    }

    PruneStoredActivityPeriods(now);
  }
  last_idle_check_ = now;
}

void DeviceStatusCollector::GetActivityTimes(
    em::DeviceStatusReportRequest* request) {
  DictionaryPrefUpdate update(local_state_, prefs::kDeviceActivityTimes);
  base::DictionaryValue* activity_times = update.Get();

  for (base::DictionaryValue::Iterator it(*activity_times); !it.IsAtEnd();
       it.Advance()) {
    int64 start_timestamp;
    int activity_milliseconds;
    if (base::StringToInt64(it.key(), &start_timestamp) &&
        it.value().GetAsInteger(&activity_milliseconds)) {
      // This is correct even when there are leap seconds, because when a leap
      // second occurs, two consecutive seconds have the same timestamp.
      int64 end_timestamp = start_timestamp + kMillisecondsPerDay;

      em::ActiveTimePeriod* active_period = request->add_active_period();
      em::TimePeriod* period = active_period->mutable_time_period();
      period->set_start_timestamp(start_timestamp);
      period->set_end_timestamp(end_timestamp);
      active_period->set_active_duration(activity_milliseconds);
      if (start_timestamp >= last_reported_day_) {
        last_reported_day_ = start_timestamp;
        duration_for_last_reported_day_ = activity_milliseconds;
      }
    } else {
      NOTREACHED();
    }
  }
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

void DeviceStatusCollector::GetLocation(
    em::DeviceStatusReportRequest* request) {
  em::DeviceLocation* location = request->mutable_device_location();
  if (!position_.Validate()) {
    location->set_error_code(
        em::DeviceLocation::ERROR_CODE_POSITION_UNAVAILABLE);
    location->set_error_message(position_.error_message);
  } else {
    location->set_latitude(position_.latitude);
    location->set_longitude(position_.longitude);
    location->set_accuracy(position_.accuracy);
    location->set_timestamp(
        (position_.timestamp - Time::UnixEpoch()).InMilliseconds());
    // Lowest point on land is at approximately -400 meters.
    if (position_.altitude > -10000.)
      location->set_altitude(position_.altitude);
    if (position_.altitude_accuracy >= 0.)
      location->set_altitude_accuracy(position_.altitude_accuracy);
    if (position_.heading >= 0. && position_.heading <= 360)
      location->set_heading(position_.heading);
    if (position_.speed >= 0.)
      location->set_speed(position_.speed);
    location->set_error_code(em::DeviceLocation::ERROR_CODE_NONE);
  }
}

void DeviceStatusCollector::GetStatus(em::DeviceStatusReportRequest* request) {
  // TODO(mnissler): Remove once the old cloud policy stack is retired. The old
  // stack doesn't support reporting successful submissions back to here, so
  // just assume whatever ends up in |request| gets submitted successfully.
  GetDeviceStatus(request);
  OnSubmittedSuccessfully();
}

bool DeviceStatusCollector::GetDeviceStatus(
    em::DeviceStatusReportRequest* status) {
  if (report_activity_times_)
    GetActivityTimes(status);

  if (report_version_info_)
    GetVersionInfo(status);

  if (report_boot_mode_)
    GetBootMode(status);

  if (report_location_)
    GetLocation(status);

  return true;
}

bool DeviceStatusCollector::GetSessionStatus(
    em::SessionStatusReportRequest* status) {
  return false;
}

void DeviceStatusCollector::OnSubmittedSuccessfully() {
  TrimStoredActivityPeriods(last_reported_day_, duration_for_last_reported_day_,
                            std::numeric_limits<int64>::max());
}

void DeviceStatusCollector::OnOSVersion(const std::string& version) {
  os_version_ = version;
}

void DeviceStatusCollector::OnOSFirmware(const std::string& version) {
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

void DeviceStatusCollector::ScheduleGeolocationUpdateRequest() {
  if (geolocation_update_timer_.IsRunning() || geolocation_update_in_progress_)
    return;

  if (position_.Validate()) {
    TimeDelta elapsed = GetCurrentTime() - position_.timestamp;
    TimeDelta interval =
        TimeDelta::FromSeconds(kGeolocationPollIntervalSeconds);
    if (elapsed > interval) {
      geolocation_update_in_progress_ = true;
      location_update_requester_(base::Bind(
          &DeviceStatusCollector::ReceiveGeolocationUpdate,
          weak_factory_.GetWeakPtr()));
    } else {
      geolocation_update_timer_.Start(
          FROM_HERE,
          interval - elapsed,
          this,
          &DeviceStatusCollector::ScheduleGeolocationUpdateRequest);
    }
  } else {
    geolocation_update_in_progress_ = true;
    location_update_requester_(base::Bind(
        &DeviceStatusCollector::ReceiveGeolocationUpdate,
        weak_factory_.GetWeakPtr()));

  }
}

void DeviceStatusCollector::ReceiveGeolocationUpdate(
    const content::Geoposition& position) {
  geolocation_update_in_progress_ = false;

  // Ignore update if device location reporting has since been disabled.
  if (!report_location_)
    return;

  if (position.Validate()) {
    position_ = position;
    base::DictionaryValue location;
    location.SetDouble(kLatitude, position.latitude);
    location.SetDouble(kLongitude, position.longitude);
    location.SetDouble(kAltitude, position.altitude);
    location.SetDouble(kAccuracy, position.accuracy);
    location.SetDouble(kAltitudeAccuracy, position.altitude_accuracy);
    location.SetDouble(kHeading, position.heading);
    location.SetDouble(kSpeed, position.speed);
    location.SetString(kTimestamp,
        base::Int64ToString(position.timestamp.ToInternalValue()));
    local_state_->Set(prefs::kDeviceLocation, location);
  }

  ScheduleGeolocationUpdateRequest();
}

}  // namespace policy
