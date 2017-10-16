// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/off_hours/device_off_hours_controller.h"

#include <utility>

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_system_clock_client.h"

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

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();
    system_clock_client_ = new chromeos::FakeSystemClockClient();
    dbus_setter_->SetSystemClockClient(base::WrapUnique(system_clock_client_));
    device_settings_service_.SetDeviceOffHoursControllerForTesting(
        base::MakeUnique<policy::off_hours::DeviceOffHoursController>());
  }

  void SetDeviceSettings() {
    device_policy_.Build();
    device_settings_test_helper_.set_device_policy(device_policy_.GetBlob());
    ReloadDeviceSettings();
  }

  void SetOffHoursProto(
      const std::vector<em::DeviceOffHoursIntervalProto>& intervals,
      const std::string& timezone,
      const std::vector<int>& ignored_policy_proto_tags) {
    em::ChromeDeviceSettingsProto& proto(device_policy_.payload());
    auto* off_hours = proto.mutable_device_off_hours();
    for (auto interval : intervals) {
      auto* cur = off_hours->add_intervals();
      *cur = interval;
    }
    off_hours->set_timezone(timezone);
    for (auto p : ignored_policy_proto_tags) {
      off_hours->add_ignored_policy_proto_tags(p);
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
    std::vector<int> ignored_policy_proto_tags = {
        3 /*DeviceGuestModeEnabled policy*/};
    SetOffHoursProto(intervals, "GMT", ignored_policy_proto_tags);
  }

  chromeos::FakeSystemClockClient* system_clock_client() {
    return system_clock_client_;
  }

 private:
  std::unique_ptr<policy::off_hours::DeviceOffHoursController>
      device_off_hours_controller_;

  // The object is owned by DeviceSettingsTestBase class.
  chromeos::FakeSystemClockClient* system_clock_client_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOffHoursControllerTest);
};

TEST_F(DeviceOffHoursControllerTest, CheckOffHoursUnset) {
  system_clock_client()->set_network_synchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();
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
  system_clock_client()->set_network_synchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();

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
  system_clock_client()->set_network_synchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();
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

TEST_F(DeviceOffHoursControllerTest, NoNetworkSynchronization) {
  system_clock_client()->set_network_synchronized(false);
  system_clock_client()->NotifyObserversSystemClockUpdated();
  enterprise_management::ChromeDeviceSettingsProto& proto(
      device_policy_.payload());
  proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(false);
  SetDeviceSettings();
  EXPECT_FALSE(device_settings_service_.device_settings()
                   ->guest_mode_enabled()
                   .guest_mode_enabled());
  CreateAndSetOffHours(base::TimeDelta(), kHour);
  EXPECT_FALSE(device_settings_service_.device_settings()
                   ->guest_mode_enabled()
                   .guest_mode_enabled());
}

// TODO(yakovleva): Add tests for PowerManagerClient observer, SuspendDone
// calls.

}  // namespace chromeos
