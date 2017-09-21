// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_off_hours_controller.h"

#include <utility>

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"

namespace em = enterprise_management;

namespace chromeos {

namespace {

constexpr em::WeeklyTimeProto_DayOfWeek kWeekdays[] = {
    em::WeeklyTimeProto::DAY_OF_WEEK_UNSPECIFIED,
    em::WeeklyTimeProto::MONDAY,
    em::WeeklyTimeProto::TUESDAY,
    em::WeeklyTimeProto::WEDNESDAY,
    em::WeeklyTimeProto::THURSDAY,
    em::WeeklyTimeProto::FRIDAY,
    em::WeeklyTimeProto::SATURDAY,
    em::WeeklyTimeProto::SUNDAY};
constexpr base::TimeDelta kHour = base::TimeDelta::FromHours(1);
constexpr base::TimeDelta kDay = base::TimeDelta::FromDays(1);

}  // namespace

class DeviceOffHoursControllerTest : public DeviceSettingsTestBase {
 protected:
  DeviceOffHoursControllerTest() {}

  void SetDeviceSettings() {
    device_policy_.Build();
    device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
    ReloadDeviceSettings();
  }

  void SetOffHoursProto(
      const std::vector<em::DeviceOffHoursIntervalProto>& intervals,
      const std::string& timezone,
      const std::vector<std::string>& ignored_policy) {
    em::ChromeDeviceSettingsProto& proto(device_policy_.payload());
    auto* off_hours = proto.mutable_device_off_hours();
    for (auto interval : intervals) {
      auto* cur = off_hours->add_intervals();
      *cur = interval;
    }
    off_hours->set_timezone(timezone);
    for (auto p : ignored_policy) {
      off_hours->add_ignored_policies(p);
    }
    SetDeviceSettings();
  }

  void UnsetOffHoursProto() {
    em::ChromeDeviceSettingsProto& proto(device_policy_.payload());
    proto.clear_device_off_hours();
    SetDeviceSettings();
  }

  em::DeviceOffHoursIntervalProto CreateInterval(int start_day_of_week,
                                                 base::TimeDelta start_time,
                                                 int end_day_of_week,
                                                 base::TimeDelta end_time) {
    em::DeviceOffHoursIntervalProto interval =
        em::DeviceOffHoursIntervalProto();
    em::WeeklyTimeProto* start = interval.mutable_start();
    em::WeeklyTimeProto* end = interval.mutable_end();
    start->set_day_of_week(kWeekdays[start_day_of_week]);
    start->set_time(start_time.InMilliseconds());
    end->set_day_of_week(kWeekdays[end_day_of_week]);
    end->set_time(end_time.InMilliseconds());
    return interval;
  }

  // Create and set "OffHours" interval which starts in |delay| time and with
  // |duration|. Zero delay means that "OffHours" mode start right now.
  void CreateAndSetOffHours(base::TimeDelta delay, base::TimeDelta duration) {
    base::Time::Exploded exploded;
    base::Time::Now().UTCExplode(&exploded);

    int cur_day_of_week = exploded.day_of_week;
    if (cur_day_of_week == 0)
      cur_day_of_week = 7;
    base::TimeDelta the_time = base::TimeDelta::FromDays(cur_day_of_week - 1) +
                               base::TimeDelta::FromHours(exploded.hour) +
                               base::TimeDelta::FromMinutes(exploded.minute) +
                               base::TimeDelta::FromSeconds(exploded.second);
    the_time += delay;
    base::TimeDelta start_time = the_time % kDay;
    int start_day_of_week = the_time.InDays() % 7 + 1;
    the_time += duration;
    base::TimeDelta end_time = the_time % kDay;
    int end_day_of_week = the_time.InDays() % 7 + 1;

    std::vector<em::DeviceOffHoursIntervalProto> intervals;
    intervals.push_back(CreateInterval(start_day_of_week, start_time,
                                       end_day_of_week, end_time));
    std::vector<std::string> ignored_policies = {"guest_mode_enabled"};
    SetOffHoursProto(intervals, "GMT", ignored_policies);
  }

 private:
  std::unique_ptr<policy::DeviceOffHoursController>
      device_off_hours_controller_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOffHoursControllerTest);
};

TEST_F(DeviceOffHoursControllerTest, CheckOffHoursUnset) {
  enterprise_management::ChromeDeviceSettingsProto& proto(
      device_policy_.payload());
  proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(false);
  SetDeviceSettings();
  EXPECT_FALSE(device_settings_service_.device_settings()
                   ->guest_mode_enabled()
                   .guest_mode_enabled());
  UnsetOffHoursProto();
  EXPECT_FALSE(device_settings_service_.device_settings()
                   ->guest_mode_enabled()
                   .guest_mode_enabled());
}

TEST_F(DeviceOffHoursControllerTest, CheckOffHoursModeOff) {
  enterprise_management::ChromeDeviceSettingsProto& proto(
      device_policy_.payload());
  proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(false);
  SetDeviceSettings();
  EXPECT_FALSE(device_settings_service_.device_settings()
                   ->guest_mode_enabled()
                   .guest_mode_enabled());
  CreateAndSetOffHours(kHour, kHour);
  EXPECT_FALSE(device_settings_service_.device_settings()
                   ->guest_mode_enabled()
                   .guest_mode_enabled());
}

TEST_F(DeviceOffHoursControllerTest, CheckOffHoursModeOn) {
  enterprise_management::ChromeDeviceSettingsProto& proto(
      device_policy_.payload());
  proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(false);
  SetDeviceSettings();
  EXPECT_FALSE(device_settings_service_.device_settings()
                   ->guest_mode_enabled()
                   .guest_mode_enabled());
  CreateAndSetOffHours(base::TimeDelta(), kHour);
  EXPECT_TRUE(device_settings_service_.device_settings()
                  ->guest_mode_enabled()
                  .guest_mode_enabled());
}

}  // namespace chromeos
