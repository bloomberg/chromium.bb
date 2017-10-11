// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_off_hours_controller.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/device_policy_remover.h"
#include "chrome/browser/chromeos/policy/off_hours/weekly_time.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace em = enterprise_management;

namespace policy {
namespace {

constexpr base::TimeDelta kDay = base::TimeDelta::FromDays(1);
constexpr base::TimeDelta kWeek = base::TimeDelta::FromDays(7);

// Put time in milliseconds which is added to local time to get GMT time to
// |offset| considering current daylight time. Return true if there was no
// error.
bool GetOffsetFromTimezoneToGmt(const std::string& timezone, int* offset) {
  auto zone = base::WrapUnique(
      icu::TimeZone::createTimeZone(icu::UnicodeString::fromUTF8(timezone)));
  if (*zone == icu::TimeZone::getUnknown()) {
    LOG(ERROR) << "Unsupported timezone: " << timezone;
    return false;
  }
  // Time in milliseconds which is added to GMT to get local time.
  int gmt_offset = zone->getRawOffset();
  // Time in milliseconds which is added to local standard time to get local
  // wall clock time.
  int dst_offset = zone->getDSTSavings();
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::GregorianCalendar> gregorian_calendar =
      base::MakeUnique<icu::GregorianCalendar>(*zone, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Gregorian calendar error = " << u_errorName(status);
    return false;
  }
  status = U_ZERO_ERROR;
  UBool day_light = gregorian_calendar->inDaylightTime(status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Daylight time error = " << u_errorName(status);
    return false;
  }
  if (day_light)
    gmt_offset += dst_offset;
  // -|gmt_offset| is time which is added to local time to get GMT time.
  *offset = -gmt_offset;
  return true;
}

// Return WeeklyTime structure from WeeklyTimeProto. Return nullptr if
// WeeklyTime structure isn't correct.
std::unique_ptr<off_hours::WeeklyTime> GetWeeklyTime(
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
  int time_of_day = container.time();
  if (!(time_of_day >= 0 && time_of_day < kDay.InMilliseconds())) {
    LOG(ERROR) << "Invalid time value: " << time_of_day
               << ", the value should be in [0; " << kDay.InMilliseconds()
               << ").";
    return nullptr;
  }
  return base::MakeUnique<off_hours::WeeklyTime>(container.day_of_week(),
                                                 time_of_day);
}

// Return list of time intervals from DeviceOffHoursProto structure.
std::vector<off_hours::OffHoursInterval> GetIntervals(
    const em::DeviceOffHoursProto& container) {
  std::vector<off_hours::OffHoursInterval> intervals;
  for (const auto& entry : container.intervals()) {
    if (!entry.has_start() || !entry.has_end()) {
      LOG(WARNING) << "Skipping interval without start or/and end.";
      continue;
    }
    auto start = GetWeeklyTime(entry.start());
    auto end = GetWeeklyTime(entry.end());
    if (start && end)
      intervals.push_back(off_hours::OffHoursInterval(*start, *end));
  }
  return intervals;
}

// Return list of proto tags of ignored policies from DeviceOffHoursProto
// structure.
std::vector<int> GetIgnoredPolicyProtoTags(
    const em::DeviceOffHoursProto& container) {
  return std::vector<int>(container.ignored_policy_proto_tags().begin(),
                          container.ignored_policy_proto_tags().end());
}

// Return timezone from DeviceOffHoursProto if exists otherwise return nullptr.
base::Optional<std::string> GetTimezone(
    const em::DeviceOffHoursProto& container) {
  if (!container.has_timezone()) {
    return base::nullopt;
  }
  return base::make_optional(container.timezone());
}

// Convert time intervals from |timezone| to GMT timezone.
std::vector<off_hours::OffHoursInterval> ConvertIntervalsToGmt(
    const std::vector<off_hours::OffHoursInterval>& intervals,
    const std::string& timezone) {
  int gmt_offset = 0;
  bool no_offset_error = GetOffsetFromTimezoneToGmt(timezone, &gmt_offset);
  if (!no_offset_error)
    return {};
  std::vector<off_hours::OffHoursInterval> gmt_intervals;
  for (const auto& interval : intervals) {
    // |gmt_offset| is added to input time to get GMT time.
    auto gmt_start = interval.start().AddMilliseconds(gmt_offset);
    auto gmt_end = interval.end().AddMilliseconds(gmt_offset);
    gmt_intervals.push_back(off_hours::OffHoursInterval(gmt_start, gmt_end));
  }
  return gmt_intervals;
}

// Return duration till next "OffHours" time interval.
base::TimeDelta GetDeltaTillNextOffHours(
    const off_hours::WeeklyTime& current_time,
    const std::vector<off_hours::OffHoursInterval>& off_hours_intervals) {
  // "OffHours" intervals repeat every week, therefore the maximum duration till
  // next "OffHours" interval is one week.
  base::TimeDelta till_off_hours = kWeek;
  for (const auto& interval : off_hours_intervals) {
    till_off_hours =
        std::min(till_off_hours, current_time.GetDurationTo(interval.start()));
  }
  return till_off_hours;
}

}  // namespace

std::unique_ptr<base::DictionaryValue> ConvertOffHoursProtoToValue(
    const em::DeviceOffHoursProto& container) {
  base::Optional<std::string> timezone = GetTimezone(container);
  if (!timezone)
    return nullptr;
  auto off_hours = base::MakeUnique<base::DictionaryValue>();
  off_hours->SetString("timezone", *timezone);
  std::vector<off_hours::OffHoursInterval> intervals = GetIntervals(container);
  auto intervals_value = base::MakeUnique<base::ListValue>();
  for (const auto& interval : intervals)
    intervals_value->Append(interval.ToValue());
  off_hours->SetList("intervals", std::move(intervals_value));
  std::vector<int> ignored_policy_proto_tags =
      GetIgnoredPolicyProtoTags(container);
  auto ignored_policies_value = base::MakeUnique<base::ListValue>();
  for (const auto& policy : ignored_policy_proto_tags)
    ignored_policies_value->GetList().emplace_back(policy);
  off_hours->SetList("ignored_policy_proto_tags",
                     std::move(ignored_policies_value));
  return off_hours;
}

std::unique_ptr<em::ChromeDeviceSettingsProto> ApplyOffHoursPolicyToProto(
    const em::ChromeDeviceSettingsProto& input_policies) {
  if (!input_policies.has_device_off_hours())
    return nullptr;
  const em::DeviceOffHoursProto& container(input_policies.device_off_hours());
  std::vector<int> ignored_policy_proto_tags =
      GetIgnoredPolicyProtoTags(container);
  std::unique_ptr<em::ChromeDeviceSettingsProto> policies =
      base::MakeUnique<em::ChromeDeviceSettingsProto>(input_policies);
  RemovePolicies(policies.get(), ignored_policy_proto_tags);
  return policies;
}

DeviceOffHoursController::DeviceOffHoursController() : weak_ptr_factory_(this) {
  // IsInitialized() check is used for testing. Otherwise it has to be already
  // initialized.
  if (chromeos::DBusThreadManager::IsInitialized()) {
    chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
        this);
    chromeos::DBusThreadManager::Get()->GetSystemClockClient()->AddObserver(
        this);
    chromeos::DBusThreadManager::Get()
        ->GetSystemClockClient()
        ->WaitForServiceToBeAvailable(
            base::Bind(&DeviceOffHoursController::SystemClockInitiallyAvailable,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

DeviceOffHoursController::~DeviceOffHoursController() {
  // IsInitialized() check is used for testing. Otherwise it has to be already
  // initialized.
  if (chromeos::DBusThreadManager::IsInitialized()) {
    chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
        this);
    chromeos::DBusThreadManager::Get()->GetSystemClockClient()->RemoveObserver(
        this);
  }
}

void DeviceOffHoursController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceOffHoursController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceOffHoursController::UpdateOffHoursPolicy(
    const em::ChromeDeviceSettingsProto& device_settings_proto) {
  std::vector<off_hours::OffHoursInterval> off_hours_intervals;
  if (device_settings_proto.has_device_off_hours()) {
    const em::DeviceOffHoursProto& container(
        device_settings_proto.device_off_hours());
    base::Optional<std::string> timezone = GetTimezone(container);
    if (timezone) {
      off_hours_intervals =
          ConvertIntervalsToGmt(GetIntervals(container), *timezone);
    }
  }
  off_hours_intervals_.swap(off_hours_intervals);
  UpdateOffHoursMode();
}

void DeviceOffHoursController::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  // Triggered when device wakes up. "OffHours" state could be changed during
  // sleep mode and should be updated after that.
  UpdateOffHoursMode();
}

void DeviceOffHoursController::NotifyOffHoursEndTimeChanged() const {
  VLOG(1) << "OffHours end time is changed to " << off_hours_end_time_;
  for (auto& observer : observers_)
    observer.OnOffHoursEndTimeChanged();
}

void DeviceOffHoursController::OffHoursModeIsChanged() const {
  VLOG(1) << "OffHours mode is changed to " << off_hours_mode_;
  chromeos::DeviceSettingsService::Get()->Load();
}

void DeviceOffHoursController::UpdateOffHoursMode() {
  if (off_hours_intervals_.empty() || !network_synchronized_) {
    if (!network_synchronized_) {
      VLOG(1) << "The system time isn't network synchronized. OffHours mode is "
                 "unavailable.";
    }
    StopOffHoursTimer();
    SetOffHoursMode(false);
    return;
  }
  off_hours::WeeklyTime current_time =
      off_hours::WeeklyTime::GetCurrentWeeklyTime();
  for (const auto& interval : off_hours_intervals_) {
    if (interval.Contains(current_time)) {
      base::TimeDelta remaining_off_hours_duration =
          current_time.GetDurationTo(interval.end());
      SetOffHoursEndTime(base::TimeTicks::Now() + remaining_off_hours_duration);
      StartOffHoursTimer(remaining_off_hours_duration);
      SetOffHoursMode(true);
      return;
    }
  }
  StartOffHoursTimer(
      GetDeltaTillNextOffHours(current_time, off_hours_intervals_));
  SetOffHoursMode(false);
}

void DeviceOffHoursController::SetOffHoursEndTime(
    base::TimeTicks off_hours_end_time) {
  if (off_hours_end_time == off_hours_end_time_)
    return;
  off_hours_end_time_ = off_hours_end_time;
  NotifyOffHoursEndTimeChanged();
}

void DeviceOffHoursController::SetOffHoursMode(bool off_hours_enabled) {
  if (off_hours_mode_ == off_hours_enabled)
    return;
  off_hours_mode_ = off_hours_enabled;
  DVLOG(1) << "OffHours mode: " << off_hours_mode_;
  if (!off_hours_mode_)
    SetOffHoursEndTime(base::TimeTicks());
  OffHoursModeIsChanged();
}

void DeviceOffHoursController::StartOffHoursTimer(base::TimeDelta delay) {
  DCHECK_GT(delay, base::TimeDelta());
  DVLOG(1) << "OffHours mode timer starts for " << delay;
  timer_.Start(FROM_HERE, delay,
               base::Bind(&DeviceOffHoursController::UpdateOffHoursMode,
                          weak_ptr_factory_.GetWeakPtr()));
}

void DeviceOffHoursController::StopOffHoursTimer() {
  timer_.Stop();
}

void DeviceOffHoursController::SystemClockUpdated() {
  // Triggered when the device time is changed. When it happens the "OffHours"
  // mode could be changed too, because "OffHours" mode directly depends on the
  // current device time. Ask SystemClockClient to update information about the
  // system time synchronization with the network time asynchronously.
  // Information will be received by NetworkSynchronizationUpdated method.
  chromeos::DBusThreadManager::Get()->GetSystemClockClient()->GetLastSyncInfo(
      base::Bind(&DeviceOffHoursController::NetworkSynchronizationUpdated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DeviceOffHoursController::SystemClockInitiallyAvailable(
    bool service_is_available) {
  if (!service_is_available)
    return;
  chromeos::DBusThreadManager::Get()->GetSystemClockClient()->GetLastSyncInfo(
      base::Bind(&DeviceOffHoursController::NetworkSynchronizationUpdated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DeviceOffHoursController::NetworkSynchronizationUpdated(
    bool network_synchronized) {
  // Triggered when information about the system time synchronization with
  // network is received.
  network_synchronized_ = network_synchronized;
  UpdateOffHoursMode();
}

}  // namespace policy
