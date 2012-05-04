// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_status_collector.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/cros_settings_provider.h"
#include "chrome/browser/chromeos/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/system/mock_statistics_provider.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgPointee;
using base::TimeDelta;
using base::Time;

namespace em = enterprise_management;

namespace {

const int64 kMillisecondsPerDay = Time::kMicrosecondsPerDay / 1000;

scoped_ptr<content::Geoposition> mock_position_to_return_next;

void SetMockPositionToReturnNext(const content::Geoposition &position) {
  mock_position_to_return_next.reset(new content::Geoposition(position));
}

void MockPositionUpdateRequester(
    const content::GeolocationUpdateCallback& callback) {
  if (!mock_position_to_return_next.get())
    return;

  // If the fix is invalid, the DeviceStatusCollector will immediately request
  // another update when it receives the callback. This is desirable and safe in
  // real life where geolocation updates arrive asynchronously. In this testing
  // harness, the callback is invoked synchronously upon request, leading to a
  // request-callback loop. The loop is broken by returning the mock position
  // only once.
  scoped_ptr<content::Geoposition> position(
      mock_position_to_return_next.release());
  callback.Run(*position);
}

class TestingDeviceStatusCollector : public policy::DeviceStatusCollector {
 public:
  TestingDeviceStatusCollector(
      PrefService* local_state,
      chromeos::system::StatisticsProvider* provider)
      : policy::DeviceStatusCollector(local_state,
                                      provider,
                                      &MockPositionUpdateRequester) {
    // Set the baseline time to a fixed value (1 AM) to prevent test flakiness
    // due to a single activity period spanning two days.
    SetBaselineTime(Time::Now().LocalMidnight() + TimeDelta::FromHours(1));
  }

  void Simulate(IdleState* states, int len) {
    for (int i = 0; i < len; i++)
      IdleStateCallback(states[i]);
  }

  void set_max_stored_past_activity_days(unsigned int value) {
    max_stored_past_activity_days_ = value;
  }

  void set_max_stored_future_activity_days(unsigned int value) {
    max_stored_future_activity_days_ = value;
  }

  // Reset the baseline time.
  void SetBaselineTime(Time time) {
    baseline_time_ = time;
    baseline_offset_periods_ = 0;
  }

 protected:
  virtual void CheckIdleState() OVERRIDE {
    // This should never be called in testing, as it results in a dbus call.
    NOTREACHED();
  }

  // Each time this is called, returns a time that is a fixed increment
  // later than the previous time.
  virtual Time GetCurrentTime() OVERRIDE {
    int poll_interval = policy::DeviceStatusCollector::kIdlePollIntervalSeconds;
    return baseline_time_ +
        TimeDelta::FromSeconds(poll_interval * baseline_offset_periods_++);
  }

 private:
  // Baseline time for the fake times returned from GetCurrentTime().
  Time baseline_time_;

  // The number of simulated periods since the baseline time.
  int baseline_offset_periods_;
};

// Return the total number of active milliseconds contained in a device
// status report.
int64 GetActiveMilliseconds(em::DeviceStatusReportRequest& status) {
  int64 active_milliseconds = 0;
  for (int i = 0; i < status.active_period_size(); i++) {
    active_milliseconds += status.active_period(i).active_duration();
  }
  return active_milliseconds;
}

}  // namespace

namespace policy {

class DeviceStatusCollectorTest : public testing::Test {
 public:
  DeviceStatusCollectorTest()
    : message_loop_(MessageLoop::TYPE_UI),
      ui_thread_(content::BrowserThread::UI, &message_loop_),
      file_thread_(content::BrowserThread::FILE, &message_loop_),
      io_thread_(content::BrowserThread::IO, &message_loop_) {
    TestingDeviceStatusCollector::RegisterPrefs(&prefs_);

    EXPECT_CALL(statistics_provider_, GetMachineStatistic(_, NotNull()))
        .WillRepeatedly(Return(false));

    // Remove the real DeviceSettingsProvider and replace it with a stub.
    cros_settings_ = chromeos::CrosSettings::Get();
    device_settings_provider_ =
        cros_settings_->GetProvider(chromeos::kReportDeviceVersionInfo);
    EXPECT_TRUE(device_settings_provider_ != NULL);
    EXPECT_TRUE(
        cros_settings_->RemoveSettingsProvider(device_settings_provider_));
    cros_settings_->AddSettingsProvider(&stub_settings_provider_);

    RestartStatusCollector();
  }

  ~DeviceStatusCollectorTest() {
    // Restore the real DeviceSettingsProvider.
    EXPECT_TRUE(
      cros_settings_->RemoveSettingsProvider(&stub_settings_provider_));
    cros_settings_->AddSettingsProvider(device_settings_provider_);
  }

  void RestartStatusCollector() {
    status_collector_.reset(
        new TestingDeviceStatusCollector(&prefs_, &statistics_provider_));
  }

  void GetStatus() {
    status_.Clear();
    status_collector_->GetStatus(&status_);
  }

  void CheckThatNoLocationIsReported() {
    GetStatus();
    EXPECT_FALSE(status_.has_device_location());
  }

  void CheckThatAValidLocationIsReported() {
    // Checks that a location is being reported which matches the valid fix
    // set using SetMockPositionToReturnNext().
    GetStatus();
    EXPECT_TRUE(status_.has_device_location());
    em::DeviceLocation location = status_.device_location();
    if (location.has_error_code())
      EXPECT_EQ(em::DeviceLocation::ERROR_CODE_NONE, location.error_code());
    EXPECT_TRUE(location.has_latitude());
    EXPECT_TRUE(location.has_longitude());
    EXPECT_TRUE(location.has_accuracy());
    EXPECT_TRUE(location.has_timestamp());
    EXPECT_FALSE(location.has_altitude());
    EXPECT_FALSE(location.has_altitude_accuracy());
    EXPECT_FALSE(location.has_heading());
    EXPECT_FALSE(location.has_speed());
    EXPECT_FALSE(location.has_error_message());
    EXPECT_DOUBLE_EQ(4.3, location.latitude());
    EXPECT_DOUBLE_EQ(-7.8, location.longitude());
    EXPECT_DOUBLE_EQ(3., location.accuracy());
    // Check that the timestamp is not older than ten minutes.
    EXPECT_TRUE(Time::Now() - Time::FromDoubleT(location.timestamp() / 1000.) <
                TimeDelta::FromMinutes(10));
  }

  void CheckThatALocationErrorIsReported() {
    GetStatus();
    EXPECT_TRUE(status_.has_device_location());
    em::DeviceLocation location = status_.device_location();
    EXPECT_TRUE(location.has_error_code());
    EXPECT_EQ(em::DeviceLocation::ERROR_CODE_POSITION_UNAVAILABLE,
              location.error_code());
  }

 protected:
  // Convenience method.
  int64 ActivePeriodMilliseconds() {
    return policy::DeviceStatusCollector::kIdlePollIntervalSeconds * 1000;
  }

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;

  TestingPrefService prefs_;
  chromeos::system::MockStatisticsProvider statistics_provider_;
  scoped_ptr<TestingDeviceStatusCollector> status_collector_;
  em::DeviceStatusReportRequest status_;
  chromeos::CrosSettings* cros_settings_;
  chromeos::CrosSettingsProvider* device_settings_provider_;
  chromeos::StubCrosSettingsProvider stub_settings_provider_;
};

TEST_F(DeviceStatusCollectorTest, AllIdle) {
  IdleState test_states[] = {
    IDLE_STATE_IDLE,
    IDLE_STATE_IDLE,
    IDLE_STATE_IDLE
  };
  cros_settings_->SetBoolean(chromeos::kReportDeviceActivityTimes, true);

  // Test reporting with no data.
  GetStatus();
  EXPECT_EQ(0, status_.active_period_size());
  EXPECT_EQ(0, GetActiveMilliseconds(status_));

  // Test reporting with a single idle sample.
  status_collector_->Simulate(test_states, 1);
  GetStatus();
  EXPECT_EQ(0, status_.active_period_size());
  EXPECT_EQ(0, GetActiveMilliseconds(status_));

  // Test reporting with multiple consecutive idle samples.
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(IdleState));
  GetStatus();
  EXPECT_EQ(0, status_.active_period_size());
  EXPECT_EQ(0, GetActiveMilliseconds(status_));
}

TEST_F(DeviceStatusCollectorTest, AllActive) {
  IdleState test_states[] = {
    IDLE_STATE_ACTIVE,
    IDLE_STATE_ACTIVE,
    IDLE_STATE_ACTIVE
  };
  cros_settings_->SetBoolean(chromeos::kReportDeviceActivityTimes, true);

  // Test a single active sample.
  status_collector_->Simulate(test_states, 1);
  GetStatus();
  EXPECT_EQ(1, status_.active_period_size());
  EXPECT_EQ(1 * ActivePeriodMilliseconds(), GetActiveMilliseconds(status_));
  status_.clear_active_period(); // Clear the result protobuf.

  // Test multiple consecutive active samples.
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(IdleState));
  GetStatus();
  EXPECT_EQ(1, status_.active_period_size());
  EXPECT_EQ(3 * ActivePeriodMilliseconds(), GetActiveMilliseconds(status_));
}

TEST_F(DeviceStatusCollectorTest, MixedStates) {
  IdleState test_states[] = {
    IDLE_STATE_ACTIVE,
    IDLE_STATE_IDLE,
    IDLE_STATE_ACTIVE,
    IDLE_STATE_ACTIVE,
    IDLE_STATE_IDLE,
    IDLE_STATE_IDLE,
    IDLE_STATE_ACTIVE
  };
  cros_settings_->SetBoolean(chromeos::kReportDeviceActivityTimes, true);
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(IdleState));
  GetStatus();
  EXPECT_EQ(4 * ActivePeriodMilliseconds(), GetActiveMilliseconds(status_));
}

TEST_F(DeviceStatusCollectorTest, StateKeptInPref) {
  IdleState test_states[] = {
    IDLE_STATE_ACTIVE,
    IDLE_STATE_IDLE,
    IDLE_STATE_ACTIVE,
    IDLE_STATE_ACTIVE,
    IDLE_STATE_IDLE,
    IDLE_STATE_IDLE
  };
  cros_settings_->SetBoolean(chromeos::kReportDeviceActivityTimes, true);
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(IdleState));

  // Process the list a second time after restarting the collector. It should be
  // able to count the active periods found by the original collector, because
  // the results are stored in a pref.
  RestartStatusCollector();
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(IdleState));

  GetStatus();
  EXPECT_EQ(6 * ActivePeriodMilliseconds(), GetActiveMilliseconds(status_));
}

TEST_F(DeviceStatusCollectorTest, Times) {
  IdleState test_states[] = {
    IDLE_STATE_ACTIVE,
    IDLE_STATE_IDLE,
    IDLE_STATE_ACTIVE,
    IDLE_STATE_ACTIVE,
    IDLE_STATE_IDLE,
    IDLE_STATE_IDLE
  };
  cros_settings_->SetBoolean(chromeos::kReportDeviceActivityTimes, true);
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(IdleState));
  GetStatus();
  EXPECT_EQ(3 * ActivePeriodMilliseconds(), GetActiveMilliseconds(status_));
}

TEST_F(DeviceStatusCollectorTest, MaxStoredPeriods) {
  IdleState test_states[] = {
    IDLE_STATE_ACTIVE,
    IDLE_STATE_IDLE
  };
  unsigned int max_days = 10;

  cros_settings_->SetBoolean(chromeos::kReportDeviceActivityTimes, true);
  status_collector_->set_max_stored_past_activity_days(max_days - 1);
  status_collector_->set_max_stored_future_activity_days(1);
  Time baseline = Time::Now().LocalMidnight();

  // Simulate 12 active periods.
  for (int i = 0; i < static_cast<int>(max_days) + 2; i++) {
    status_collector_->Simulate(test_states,
                                sizeof(test_states) / sizeof(IdleState));
    // Advance the simulated clock by a day.
    baseline += TimeDelta::FromDays(1);
    status_collector_->SetBaselineTime(baseline);
  }

  // Check that we don't exceed the max number of periods.
  GetStatus();
  EXPECT_EQ(static_cast<int>(max_days), status_.active_period_size());

  // Simulate some future times.
  for (int i = 0; i < static_cast<int>(max_days) + 2; i++) {
    status_collector_->Simulate(test_states,
                                sizeof(test_states) / sizeof(IdleState));
    // Advance the simulated clock by a day.
    baseline += TimeDelta::FromDays(1);
    status_collector_->SetBaselineTime(baseline);
  }
  // Set the clock back so the previous simulated times are in the future.
  baseline -= TimeDelta::FromDays(20);
  status_collector_->SetBaselineTime(baseline);

  // Collect one more data point to trigger pruning.
  status_collector_->Simulate(test_states, 1);

  // Check that we don't exceed the max number of periods.
  status_.clear_active_period();
  GetStatus();
  EXPECT_LT(status_.active_period_size(), static_cast<int>(max_days));
}

TEST_F(DeviceStatusCollectorTest, ActivityTimesDisabledByDefault) {
  // If the pref for collecting device activity times isn't explicitly turned
  // on, no data on activity times should be reported.

  IdleState test_states[] = {
    IDLE_STATE_ACTIVE,
    IDLE_STATE_ACTIVE,
    IDLE_STATE_ACTIVE
  };
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(IdleState));
  GetStatus();
  EXPECT_EQ(0, status_.active_period_size());
  EXPECT_EQ(0, GetActiveMilliseconds(status_));
}

TEST_F(DeviceStatusCollectorTest, ActivityCrossingMidnight) {
  IdleState test_states[] = {
    IDLE_STATE_ACTIVE
  };
  cros_settings_->SetBoolean(chromeos::kReportDeviceActivityTimes, true);

  // Set the baseline time to 10 seconds after midnight.
  status_collector_->SetBaselineTime(
      Time::Now().LocalMidnight() + TimeDelta::FromSeconds(10));

  status_collector_->Simulate(test_states, 1);
  GetStatus();
  ASSERT_EQ(2, status_.active_period_size());

  em::ActiveTimePeriod period0 = status_.active_period(0);
  em::ActiveTimePeriod period1 = status_.active_period(1);
  EXPECT_EQ(ActivePeriodMilliseconds() - 10000, period0.active_duration());
  EXPECT_EQ(10000, period1.active_duration());

  em::TimePeriod time_period0 = period0.time_period();
  em::TimePeriod time_period1 = period1.time_period();

  EXPECT_EQ(time_period0.end_timestamp(), time_period1.start_timestamp());

  // Ensure that the start and end times for the period are a day apart.
  EXPECT_EQ(time_period0.end_timestamp() - time_period0.start_timestamp(),
            kMillisecondsPerDay);
  EXPECT_EQ(time_period1.end_timestamp() - time_period1.start_timestamp(),
            kMillisecondsPerDay);
}

TEST_F(DeviceStatusCollectorTest, DevSwitchBootMode) {
  // Test that boot mode data is not reported if the pref is not turned on.
  EXPECT_CALL(statistics_provider_,
              GetMachineStatistic("devsw_boot", NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>("0"), Return(true)));
  GetStatus();
  EXPECT_FALSE(status_.has_boot_mode());

  // Turn the pref on, and check that the status is reported iff the
  // statistics provider returns valid data.
  cros_settings_->SetBoolean(chromeos::kReportDeviceBootMode, true);

  EXPECT_CALL(statistics_provider_,
              GetMachineStatistic("devsw_boot", NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("(error)"), Return(true)));
  GetStatus();
  EXPECT_FALSE(status_.has_boot_mode());

  EXPECT_CALL(statistics_provider_,
              GetMachineStatistic("devsw_boot", NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>(" "), Return(true)));
  GetStatus();
  EXPECT_FALSE(status_.has_boot_mode());

  EXPECT_CALL(statistics_provider_,
              GetMachineStatistic("devsw_boot", NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("0"), Return(true)));
  GetStatus();
  EXPECT_EQ("Verified", status_.boot_mode());

  EXPECT_CALL(statistics_provider_,
              GetMachineStatistic("devsw_boot", NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("1"), Return(true)));
  GetStatus();
  EXPECT_EQ("Dev", status_.boot_mode());
}

TEST_F(DeviceStatusCollectorTest, VersionInfo) {
  // When the pref to collect this data is not enabled, expect that none of
  // the fields are present in the protobuf.
  GetStatus();
  EXPECT_FALSE(status_.has_browser_version());
  EXPECT_FALSE(status_.has_os_version());
  EXPECT_FALSE(status_.has_firmware_version());

  cros_settings_->SetBoolean(chromeos::kReportDeviceVersionInfo, true);
  GetStatus();
  EXPECT_TRUE(status_.has_browser_version());
  EXPECT_TRUE(status_.has_os_version());
  EXPECT_TRUE(status_.has_firmware_version());

  // Check that the browser version is not empty. OS version & firmware
  // don't have any reasonable values inside the unit test, so those
  // aren't checked.
  EXPECT_NE("", status_.browser_version());
}

TEST_F(DeviceStatusCollectorTest, Location) {
  content::Geoposition valid_fix;
  valid_fix.latitude = 4.3;
  valid_fix.longitude = -7.8;
  valid_fix.accuracy = 3.;
  valid_fix.timestamp = Time::Now();

  content::Geoposition invalid_fix;
  invalid_fix.error_code =
      content::Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
  invalid_fix.timestamp = Time::Now();

  // Check that when device location reporting is disabled, no location is
  // reported.
  SetMockPositionToReturnNext(valid_fix);
  CheckThatNoLocationIsReported();

  // Check that when device location reporting is enabled and a valid fix is
  // available, the location is reported and is stored in local state.
  SetMockPositionToReturnNext(valid_fix);
  cros_settings_->SetBoolean(chromeos::kReportDeviceLocation, true);
  EXPECT_FALSE(prefs_.GetDictionary(prefs::kDeviceLocation)->empty());
  CheckThatAValidLocationIsReported();

  // Restart the status collector. Check that the last known location has been
  // retrieved from local state without requesting a geolocation update.
  SetMockPositionToReturnNext(valid_fix);
  RestartStatusCollector();
  CheckThatAValidLocationIsReported();
  EXPECT_TRUE(mock_position_to_return_next.get());

  // Check that after disabling location reporting again, the last known
  // location has been cleared from local state and is no longer reported.
  SetMockPositionToReturnNext(valid_fix);
  cros_settings_->SetBoolean(chromeos::kReportDeviceLocation, false);
  // Allow the new pref to propagate to the status collector.
  message_loop_.RunAllPending();
  EXPECT_TRUE(prefs_.GetDictionary(prefs::kDeviceLocation)->empty());
  CheckThatNoLocationIsReported();

  // Check that after enabling location reporting again, an error is reported
  // if no valid fix is available.
  SetMockPositionToReturnNext(invalid_fix);
  cros_settings_->SetBoolean(chromeos::kReportDeviceLocation, true);
  // Allow the new pref to propagate to the status collector.
  message_loop_.RunAllPending();
  CheckThatALocationErrorIsReported();
}

}  // namespace policy
