// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_status_collector.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_version_info.h"

using base::Time;
using base::TimeDelta;
using chromeos::VersionLoader;

namespace em = enterprise_management;

namespace {
// How many seconds of inactivity triggers the idle state.
const unsigned int kIdleStateThresholdSeconds = 300;

// The maximum number of time periods stored in the local state.
const unsigned int kMaxStoredActivePeriods = 500;

// Stores the baseline timestamp, to which the active periods are relative.
const char* const kPrefBaselineTime = "device_status.baseline_timestamp";

// Stores a list of timestamps representing device active periods.
const char* const kPrefDeviceActivePeriods = "device_status.active_periods";

bool GetTimestamp(const ListValue* list, int index, int64* out_value) {
  std::string string_value;
  if (list->GetString(index, &string_value))
    return base::StringToInt64(string_value, out_value);
  return false;
}

}  // namespace

namespace policy {

DeviceStatusCollector::DeviceStatusCollector(
    PrefService* local_state,
    chromeos::system::StatisticsProvider* provider)
    : max_stored_active_periods_(kMaxStoredActivePeriods),
      local_state_(local_state),
      last_idle_check_(Time()),
      last_idle_state_(IDLE_STATE_UNKNOWN),
      statistics_provider_(provider) {
  timer_.Start(FROM_HERE,
               TimeDelta::FromSeconds(
                   DeviceStatusCollector::kPollIntervalSeconds),
               this, &DeviceStatusCollector::CheckIdleState);

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
}

// static
void DeviceStatusCollector::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterInt64Pref(kPrefBaselineTime, Time::Now().ToTimeT());
  local_state->RegisterListPref(kPrefDeviceActivePeriods, new ListValue);
}

void DeviceStatusCollector::CheckIdleState() {
  CalculateIdleState(kIdleStateThresholdSeconds,
      base::Bind(&DeviceStatusCollector::IdleStateCallback,
                 base::Unretained(this)));
}

Time DeviceStatusCollector::GetCurrentTime() {
  return Time::Now();
}

void DeviceStatusCollector::AddActivePeriod(Time start, Time end) {
  // Maintain the list of active periods in a local_state pref.
  ListPrefUpdate update(local_state_, kPrefDeviceActivePeriods);
  ListValue* active_periods = update.Get();

  // Cap the number of active periods that we store.
  if (active_periods->GetSize() >= 2 * max_stored_active_periods_)
    return;

  Time epoch = Time::UnixEpoch();
  int64 start_timestamp = (start - epoch).InMilliseconds();
  Value* end_value = new StringValue(
      base::Int64ToString((end - epoch).InMilliseconds()));

  int list_size = active_periods->GetSize();
  DCHECK(list_size % 2 == 0);

  // Check if this period can be combined with the previous one.
  if (list_size > 0 && last_idle_state_ == IDLE_STATE_ACTIVE) {
    int64 last_period_end;
    if (GetTimestamp(active_periods, list_size - 1, &last_period_end) &&
        last_period_end == start_timestamp) {
      active_periods->Set(list_size - 1, end_value);
      return;
    }
  }
  // Add a new period to the list.
  active_periods->Append(
      new StringValue(base::Int64ToString(start_timestamp)));
  active_periods->Append(end_value);
}

void DeviceStatusCollector::IdleStateCallback(IdleState state) {
  Time now = GetCurrentTime();

  if (state == IDLE_STATE_ACTIVE) {
    unsigned int poll_interval = DeviceStatusCollector::kPollIntervalSeconds;

    // If it's been too long since the last report, assume that the system was
    // in standby, and only count a single interval of activity.
    if ((now - last_idle_check_).InSeconds() >= (2 * poll_interval))
      AddActivePeriod(now - TimeDelta::FromSeconds(poll_interval), now);
    else
      AddActivePeriod(last_idle_check_, now);
  }
  last_idle_check_ = now;
  last_idle_state_ = state;
}

void DeviceStatusCollector::GetStatus(em::DeviceStatusReportRequest* request) {
  // Report device active periods.
  const ListValue* active_periods =
      local_state_->GetList(kPrefDeviceActivePeriods);
  em::TimePeriod* time_period;

  DCHECK(active_periods->GetSize() % 2 == 0);

  int period_count = active_periods->GetSize() / 2;
  for (int i = 0; i < period_count; i++) {
    int64 start, end;

    if (!GetTimestamp(active_periods, 2 * i, &start) ||
        !GetTimestamp(active_periods, 2 * i + 1, &end) ||
        end < start) {
      // Something is amiss -- bail out.
      NOTREACHED();
      break;
    }
    time_period = request->add_active_time();
    time_period->set_start_timestamp(start);
    time_period->set_end_timestamp(end);
  }
  ListPrefUpdate update(local_state_, kPrefDeviceActivePeriods);
  update.Get()->Clear();

  chrome::VersionInfo version_info;
  request->set_browser_version(version_info.Version());
  request->set_os_version(os_version_);
  request->set_firmware_version(firmware_version_);

  // Report the state of the dev switch at boot.
  std::string dev_switch_mode;
  if (statistics_provider_->GetMachineStatistic(
      "devsw_boot", &dev_switch_mode)) {
    if (dev_switch_mode == "1")
      request->set_boot_mode("Dev");
    else if (dev_switch_mode == "0")
      request->set_boot_mode("Verified");
  }
}

void DeviceStatusCollector::OnOSVersion(VersionLoader::Handle handle,
                                        std::string version) {
  os_version_ = version;
}

void DeviceStatusCollector::OnOSFirmware(VersionLoader::Handle handle,
                                         std::string version) {
  firmware_version_ = version;
}

}  // namespace policy
