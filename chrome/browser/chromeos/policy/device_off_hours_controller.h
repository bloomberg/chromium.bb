// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_OFF_HOURS_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_OFF_HOURS_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/off_hours/off_hours_interval.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chromeos/dbus/power_manager_client.h"

namespace policy {

// Return DictionaryValue in format:
// { "timezone" : string,
//   "intervals" : list of "OffHours" Intervals,
//   "ignored_policy_proto_tags" : integer list }
// "OffHours" Interval dictionary format:
// { "start" : WeeklyTime,
//   "end" : WeeklyTime }
// WeeklyTime dictionary format:
// { "day_of_week" : int # value is from 1 to 7 (1 = Monday, 2 = Tuesday, etc.)
//   "time" : int # in milliseconds from the beginning of the day.
// }
// This function is used by device_policy_decoder_chromeos to save "OffHours"
// policy in PolicyMap.
std::unique_ptr<base::DictionaryValue> ConvertOffHoursProtoToValue(
    const enterprise_management::DeviceOffHoursProto& container);

// Apply "OffHours" policy for proto which contains device policies. Return
// ChromeDeviceSettingsProto without policies from |ignored_policy_proto_tags|.
// The system will revert to the default behavior for the removed policies.
std::unique_ptr<enterprise_management::ChromeDeviceSettingsProto>
ApplyOffHoursPolicyToProto(
    const enterprise_management::ChromeDeviceSettingsProto& input_policies);

// The main class for handling "OffHours" policy turns "OffHours" mode on and
// off, handles server and client time, timezone.
//
// DeviceSettingsService is owner of this object. Use to get a reference:
// DeviceSettingsService::Get()->device_off_hours_controller()
//
// Device setting proto is changed in DeviceSettingsService and doesn't contain
// ignored device policies from DeviceOffHoursProto during "OffHours" mode. It
// is changed exactly in DeviceSettingsService because it's late to change
// policies in PrefValueMap and PolicyMap. The system will revert to the default
// behavior for the removed policies. And behavior of policies is handled during
// decoding process from proto to PolicyMap.
class DeviceOffHoursController : public chromeos::PowerManagerClient::Observer {
 public:
  // Creates a device off hours controller instance.
  DeviceOffHoursController();
  ~DeviceOffHoursController() override;

  // Return current "OffHours" mode status.
  bool IsOffHoursMode();

  // Save actual "OffHours" intervals from |device_settings_proto| to
  // |off_hours_intervals_| and call "UpdateOffhoursMode()".
  void UpdateOffHoursPolicy(
      const enterprise_management::ChromeDeviceSettingsProto&
          device_settings_proto);

  // chromeos::PowerManagerClient::Observer:
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

 private:
  // Call when "OffHours" mode is changed and ask DeviceSettingsService to
  // update current proto.
  void OffHoursModeIsChanged();

  // Check if "OffHours" mode is in current time. Update current status in
  // |off_hours_mode_|. Set timer to next update "OffHours" mode.
  void UpdateOffHoursMode();

  // Turn on and off "OffHours" mode and call "OffHoursModeIsChanged()" if
  // "OffHours" mode is changed.
  void SetOffHoursMode(bool off_hours_enabled);

  // Timer for update "OffHours" mode.
  void StartOffHoursTimer(base::TimeDelta delay);
  void StopOffHoursTimer();

  // Timer for updating device settings at the begin of next “OffHours” interval
  // or at the end of current "OffHours" interval.
  base::OneShotTimer timer_;

  bool off_hours_mode_ = false;

  // Current "OffHours" time intervals.
  std::vector<off_hours::OffHoursInterval> off_hours_intervals_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOffHoursController);
};
}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_OFF_HOURS_CONTROLLER_H_
