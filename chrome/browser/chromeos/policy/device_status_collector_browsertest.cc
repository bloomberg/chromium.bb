// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_status_collector.h"

#include "base/environment.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/stub_enterprise_install_attributes.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "chromeos/system/mock_statistics_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/geolocation_provider.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::_;
using base::Time;
using base::TimeDelta;

namespace em = enterprise_management;

namespace {

const int64 kMillisecondsPerDay = Time::kMicrosecondsPerDay / 1000;

scoped_ptr<content::Geoposition> mock_position_to_return_next;

void SetMockPositionToReturnNext(const content::Geoposition &position) {
  mock_position_to_return_next.reset(new content::Geoposition(position));
}

void MockPositionUpdateRequester(
    const content::GeolocationProvider::LocationUpdateCallback& callback) {
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
      chromeos::system::StatisticsProvider* provider,
      policy::DeviceStatusCollector::LocationUpdateRequester*
          location_update_requester)
      : policy::DeviceStatusCollector(
          local_state,
          provider,
          location_update_requester) {
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
    ADD_FAILURE();
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

// Though it is a unit test, this test is linked with browser_tests so that it
// runs in a separate process. The intention is to avoid overriding the timezone
// environment variable for other tests.
class DeviceStatusCollectorTest : public testing::Test {
 public:
  DeviceStatusCollectorTest()
    : ui_thread_(content::BrowserThread::UI, &message_loop_),
      file_thread_(content::BrowserThread::FILE, &message_loop_),
      io_thread_(content::BrowserThread::IO, &message_loop_),
      install_attributes_("managed.com",
                          "user@managed.com",
                          "device_id",
                          DEVICE_MODE_ENTERPRISE),
      user_manager_(new chromeos::MockUserManager()),
      user_manager_enabler_(user_manager_) {
    // Run this test with a well-known timezone so that Time::LocalMidnight()
    // returns the same values on all machines.
    scoped_ptr<base::Environment> env(base::Environment::Create());
    env->SetVar("TZ", "UTC");

    TestingDeviceStatusCollector::RegisterPrefs(prefs_.registry());

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

  virtual ~DeviceStatusCollectorTest() {
    // Finish pending tasks.
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    message_loop_.RunUntilIdle();

    // Restore the real DeviceSettingsProvider.
    EXPECT_TRUE(
      cros_settings_->RemoveSettingsProvider(&stub_settings_provider_));
    cros_settings_->AddSettingsProvider(device_settings_provider_);
  }

  virtual void SetUp() OVERRIDE {
    // Disable network interface reporting since it requires additional setup.
    cros_settings_->SetBoolean(chromeos::kReportDeviceNetworkInterfaces, false);
  }

  void RestartStatusCollector() {
    policy::DeviceStatusCollector::LocationUpdateRequester callback =
        base::Bind(&MockPositionUpdateRequester);
    status_collector_.reset(
        new TestingDeviceStatusCollector(&prefs_,
                                         &statistics_provider_,
                                         &callback));
  }

  void GetStatus() {
    status_.Clear();
    status_collector_->GetDeviceStatus(&status_);
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

  // Since this is a unit test running in browser_tests we must do additional
  // unit test setup and make a TestingBrowserProcess. Must be first member.
  TestingBrowserProcessInitializer initializer_;
  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;

  ScopedStubEnterpriseInstallAttributes install_attributes_;
  TestingPrefServiceSimple prefs_;
  chromeos::system::MockStatisticsProvider statistics_provider_;
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::CrosSettings* cros_settings_;
  chromeos::CrosSettingsProvider* device_settings_provider_;
  chromeos::StubCrosSettingsProvider stub_settings_provider_;
  chromeos::MockUserManager* user_manager_;
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;
  em::DeviceStatusReportRequest status_;
  scoped_ptr<TestingDeviceStatusCollector> status_collector_;
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
  EXPECT_EQ(4 * ActivePeriodMilliseconds(), GetActiveMilliseconds(status_));
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
  const int kMaxDays = 10;

  cros_settings_->SetBoolean(chromeos::kReportDeviceActivityTimes, true);
  status_collector_->set_max_stored_past_activity_days(kMaxDays - 1);
  status_collector_->set_max_stored_future_activity_days(1);
  Time baseline = Time::Now().LocalMidnight();

  // Simulate 12 active periods.
  for (int i = 0; i < kMaxDays + 2; i++) {
    status_collector_->Simulate(test_states,
                                sizeof(test_states) / sizeof(IdleState));
    // Advance the simulated clock by a day.
    baseline += TimeDelta::FromDays(1);
    status_collector_->SetBaselineTime(baseline);
  }

  // Check that we don't exceed the max number of periods.
  GetStatus();
  EXPECT_EQ(kMaxDays - 1, status_.active_period_size());

  // Simulate some future times.
  for (int i = 0; i < kMaxDays + 2; i++) {
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
  EXPECT_LT(status_.active_period_size(), kMaxDays);
}

TEST_F(DeviceStatusCollectorTest, ActivityTimesEnabledByDefault) {
  // Device activity times should be reported by default.
  IdleState test_states[] = {
    IDLE_STATE_ACTIVE,
    IDLE_STATE_ACTIVE,
    IDLE_STATE_ACTIVE
  };
  status_collector_->Simulate(test_states,
                              sizeof(test_states) / sizeof(IdleState));
  GetStatus();
  EXPECT_EQ(1, status_.active_period_size());
  EXPECT_EQ(3 * ActivePeriodMilliseconds(), GetActiveMilliseconds(status_));
}

TEST_F(DeviceStatusCollectorTest, ActivityTimesOff) {
  // Device activity times should not be reported if explicitly disabled.
  cros_settings_->SetBoolean(chromeos::kReportDeviceActivityTimes, false);

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

TEST_F(DeviceStatusCollectorTest, ActivityTimesKeptUntilSubmittedSuccessfully) {
  IdleState test_states[] = {
    IDLE_STATE_ACTIVE,
    IDLE_STATE_ACTIVE,
  };
  cros_settings_->SetBoolean(chromeos::kReportDeviceActivityTimes, true);

  status_collector_->Simulate(test_states, 2);
  GetStatus();
  EXPECT_EQ(2 * ActivePeriodMilliseconds(), GetActiveMilliseconds(status_));
  em::DeviceStatusReportRequest first_status(status_);

  // The collector returns the same status again.
  GetStatus();
  EXPECT_EQ(first_status.SerializeAsString(), status_.SerializeAsString());

  // After indicating a successful submit, the submitted status gets cleared,
  // but what got collected meanwhile sticks around.
  status_collector_->Simulate(test_states, 1);
  status_collector_->OnSubmittedSuccessfully();
  GetStatus();
  EXPECT_EQ(ActivePeriodMilliseconds(), GetActiveMilliseconds(status_));
}

TEST_F(DeviceStatusCollectorTest, DevSwitchBootMode) {
  // Test that boot mode data is reported by default.
  EXPECT_CALL(statistics_provider_,
              GetMachineStatistic("devsw_boot", NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("0"), Return(true)));
  GetStatus();
  EXPECT_EQ("Verified", status_.boot_mode());

  // Test that boot mode data is not reported if the pref turned off.
  cros_settings_->SetBoolean(chromeos::kReportDeviceBootMode, false);

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
  // Expect the version info to be reported by default.
  GetStatus();
  EXPECT_TRUE(status_.has_browser_version());
  EXPECT_TRUE(status_.has_os_version());
  EXPECT_TRUE(status_.has_firmware_version());

  // When the pref to collect this data is not enabled, expect that none of
  // the fields are present in the protobuf.
  cros_settings_->SetBoolean(chromeos::kReportDeviceVersionInfo, false);
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
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(prefs_.GetDictionary(prefs::kDeviceLocation)->empty());
  CheckThatNoLocationIsReported();

  // Check that after enabling location reporting again, an error is reported
  // if no valid fix is available.
  SetMockPositionToReturnNext(invalid_fix);
  cros_settings_->SetBoolean(chromeos::kReportDeviceLocation, true);
  // Allow the new pref to propagate to the status collector.
  message_loop_.RunUntilIdle();
  CheckThatALocationErrorIsReported();
}

TEST_F(DeviceStatusCollectorTest, ReportUsers) {
  user_manager_->CreatePublicAccountUser("public@localhost");
  user_manager_->AddUser("user0@managed.com");
  user_manager_->AddUser("user1@managed.com");
  user_manager_->AddUser("user2@managed.com");
  user_manager_->AddUser("user3@unmanaged.com");
  user_manager_->AddUser("user4@managed.com");
  user_manager_->AddUser("user5@managed.com");

  // Verify that users are reported by default.
  GetStatus();
  EXPECT_EQ(6, status_.user_size());

  // Verify that users are reported after enabling the setting.
  cros_settings_->SetBoolean(chromeos::kReportDeviceUsers, true);
  GetStatus();
  EXPECT_EQ(6, status_.user_size());
  EXPECT_EQ(em::DeviceUser::USER_TYPE_MANAGED, status_.user(0).type());
  EXPECT_EQ("user0@managed.com", status_.user(0).email());
  EXPECT_EQ(em::DeviceUser::USER_TYPE_MANAGED, status_.user(1).type());
  EXPECT_EQ("user1@managed.com", status_.user(1).email());
  EXPECT_EQ(em::DeviceUser::USER_TYPE_MANAGED, status_.user(2).type());
  EXPECT_EQ("user2@managed.com", status_.user(2).email());
  EXPECT_EQ(em::DeviceUser::USER_TYPE_UNMANAGED, status_.user(3).type());
  EXPECT_FALSE(status_.user(3).has_email());
  EXPECT_EQ(em::DeviceUser::USER_TYPE_MANAGED, status_.user(4).type());
  EXPECT_EQ("user4@managed.com", status_.user(4).email());
  EXPECT_EQ(em::DeviceUser::USER_TYPE_MANAGED, status_.user(5).type());
  EXPECT_EQ("user5@managed.com", status_.user(5).email());

  // Verify that users are no longer reported if setting is disabled.
  cros_settings_->SetBoolean(chromeos::kReportDeviceUsers, false);
  GetStatus();
  EXPECT_EQ(0, status_.user_size());
}

// Fake device state.
struct FakeDeviceData {
  const char* device_path;
  const char* type;
  const char* object_path;
  const char* mac_address;
  const char* meid;
  const char* imei;
  int expected_type; // proto enum type value, -1 for not present.
};

static const FakeDeviceData kFakeDevices[] = {
  { "/device/ethernet", shill::kTypeEthernet, "ethernet",
    "112233445566", "", "",
    em::NetworkInterface::TYPE_ETHERNET },
  { "/device/cellular1", shill::kTypeCellular, "cellular1",
    "abcdefabcdef", "A10000009296F2", "",
    em::NetworkInterface::TYPE_CELLULAR },
  { "/device/cellular2", shill::kTypeCellular, "cellular2",
    "abcdefabcdef", "", "352099001761481",
    em::NetworkInterface::TYPE_CELLULAR },
  { "/device/wifi", shill::kTypeWifi, "wifi",
    "aabbccddeeff", "", "",
    em::NetworkInterface::TYPE_WIFI },
  { "/device/bluetooth", shill::kTypeBluetooth, "bluetooth",
    "", "", "",
    em::NetworkInterface::TYPE_BLUETOOTH },
  { "/device/vpn", shill::kTypeVPN, "vpn",
    "", "", "",
    -1 },
};

class DeviceStatusCollectorNetworkInterfacesTest
    : public DeviceStatusCollectorTest {
 protected:
  virtual void SetUp() OVERRIDE {
    chromeos::DBusThreadManager::InitializeWithStub();
    chromeos::NetworkHandler::Initialize();
    chromeos::ShillDeviceClient::TestInterface* test_device_client =
        chromeos::DBusThreadManager::Get()->GetShillDeviceClient()->
            GetTestInterface();
    test_device_client->ClearDevices();
    for (size_t i = 0; i < arraysize(kFakeDevices); ++i) {
      const FakeDeviceData& dev = kFakeDevices[i];
      test_device_client->AddDevice(dev.device_path, dev.type,
                                    dev.object_path);
      if (*dev.mac_address) {
        test_device_client->SetDeviceProperty(
            dev.device_path, shill::kAddressProperty,
            base::StringValue(dev.mac_address));
      }
      if (*dev.meid) {
        test_device_client->SetDeviceProperty(
            dev.device_path, shill::kMeidProperty,
            base::StringValue(dev.meid));
      }
      if (*dev.imei) {
        test_device_client->SetDeviceProperty(
            dev.device_path, shill::kImeiProperty,
            base::StringValue(dev.imei));
      }
    }

    // Flush out pending state updates.
    base::RunLoop().RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    chromeos::NetworkHandler::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
  }
};

TEST_F(DeviceStatusCollectorNetworkInterfacesTest, NetworkInterfaces) {
  // Interfaces should be reported by default.
  GetStatus();
  EXPECT_TRUE(status_.network_interface_size() > 0);

  // No interfaces should be reported if the policy is off.
  cros_settings_->SetBoolean(chromeos::kReportDeviceNetworkInterfaces, false);
  GetStatus();
  EXPECT_EQ(0, status_.network_interface_size());

  // Switch the policy on and verify the interface list is present.
  cros_settings_->SetBoolean(chromeos::kReportDeviceNetworkInterfaces, true);
  GetStatus();

  int count = 0;
  for (size_t i = 0; i < arraysize(kFakeDevices); ++i) {
    const FakeDeviceData& dev = kFakeDevices[i];
    if (dev.expected_type == -1)
      continue;

    // Find the corresponding entry in reporting data.
    bool found_match = false;
    google::protobuf::RepeatedPtrField<em::NetworkInterface>::const_iterator
        iface;
    for (iface = status_.network_interface().begin();
         iface != status_.network_interface().end();
         ++iface) {
      // Check whether type, field presence and field values match.
      if (dev.expected_type == iface->type() &&
          iface->has_mac_address() == !!*dev.mac_address &&
          iface->has_meid() == !!*dev.meid &&
          iface->has_imei() == !!*dev.imei &&
          iface->mac_address() == dev.mac_address &&
          iface->meid() == dev.meid &&
          iface->imei() == dev.imei) {
        found_match = true;
        break;
      }
    }

    EXPECT_TRUE(found_match) << "No matching interface for fake device " << i;
    count++;
  }

  EXPECT_EQ(count, status_.network_interface_size());
}

}  // namespace policy
