// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_status_collector.h"

#include "base/message_loop.h"
#include "base/time.h"
#include "chrome/browser/idle.h"
#include "chrome/browser/chromeos/system/mock_statistics_provider.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/test/base/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::Time;

namespace em = enterprise_management;

namespace {

class TestingDeviceStatusCollector : public policy::DeviceStatusCollector {
 public:
  TestingDeviceStatusCollector(
      PrefService* local_state,
      chromeos::system::StatisticsProvider* provider)
    :  policy::DeviceStatusCollector(local_state, provider),
       local_state_(local_state),
       baseline_time_(Time::Now()) {
  }

  void Simulate(IdleState* states, int len) {
    for (int i = 0; i < len; i++)
      IdleStateCallback(states[i]);
  }

  void SimulateWithSleep(IdleState* states, int len, int ) {
    for (int i = 0; i < len; i++)
      IdleStateCallback(states[i]);
  }

  void set_max_stored_active_periods(unsigned int value) {
    max_stored_active_periods_ = value;
  }

 protected:
  virtual void CheckIdleState() OVERRIDE {
    // This should never be called in testing, as it results in a dbus call.
    NOTREACHED();
  }

  // Each time this is called, returns a time that is a fixed increment
  // later than the previous time.
  virtual Time GetCurrentTime() OVERRIDE {
    static int call_count = 0;
    return baseline_time_ + TimeDelta::FromSeconds(
        policy::DeviceStatusCollector::kPollIntervalSeconds * call_count++);
  }

 private:
  PrefService* local_state_;

  // Baseline time for the fake times returned from GetCurrentTime().
  // It doesn't really matter what this is, as long as it stays the same over
  // the lifetime of the object.
  Time baseline_time_;
};

// Return the total number of active milliseconds contained in a device
// status report.
int64 GetActiveMilliseconds(em::DeviceStatusReportRequest& status) {
  int64 active_milliseconds = 0;
  for (int i = 0; i < status.active_time_size(); i++) {
    const em::TimePeriod& period = status.active_time(i);
    active_milliseconds += period.end_timestamp() - period.start_timestamp();
  }
  return active_milliseconds;
}

}  // namespace

namespace policy {

using ::testing::_;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgPointee;

class DeviceStatusCollectorTest : public testing::Test {
 public:
  DeviceStatusCollectorTest()
    :  prefs_(),
       status_collector_(&prefs_, &statistics_provider_) {
    DeviceStatusCollector::RegisterPrefs(&prefs_);
    EXPECT_CALL(statistics_provider_, GetMachineStatistic(_, NotNull()))
        .WillRepeatedly(Return(false));
  }

 protected:
  // Convenience method.
  int64 ActivePeriodMilliseconds() {
    return policy::DeviceStatusCollector::kPollIntervalSeconds * 1000;
  }

  MessageLoop message_loop_;
  TestingPrefService prefs_;
  chromeos::system::MockStatisticsProvider statistics_provider_;
  TestingDeviceStatusCollector status_collector_;
  em::DeviceStatusReportRequest status_;
};

TEST_F(DeviceStatusCollectorTest, AllIdle) {
  IdleState test_states[] = {
    IDLE_STATE_IDLE,
    IDLE_STATE_IDLE,
    IDLE_STATE_IDLE
  };
  // Test reporting with no data.
  status_collector_.GetStatus(&status_);
  EXPECT_EQ(0, status_.active_time_size());
  EXPECT_EQ(0, GetActiveMilliseconds(status_));

  // Test reporting with a single idle sample.
  status_collector_.Simulate(test_states, 1);
  status_collector_.GetStatus(&status_);
  EXPECT_EQ(0, status_.active_time_size());
  EXPECT_EQ(0, GetActiveMilliseconds(status_));

  // Test reporting with multiple consecutive idle samples.
  status_collector_.Simulate(test_states,
                             sizeof(test_states) / sizeof(IdleState));
  status_collector_.GetStatus(&status_);
  EXPECT_EQ(0, status_.active_time_size());
  EXPECT_EQ(0, GetActiveMilliseconds(status_));
}

TEST_F(DeviceStatusCollectorTest, AllActive) {
  IdleState test_states[] = {
    IDLE_STATE_ACTIVE,
    IDLE_STATE_ACTIVE,
    IDLE_STATE_ACTIVE
  };
  // Test a single active sample.
  status_collector_.Simulate(test_states, 1);
  status_collector_.GetStatus(&status_);
  EXPECT_EQ(1, status_.active_time_size());
  EXPECT_EQ(1 * ActivePeriodMilliseconds(), GetActiveMilliseconds(status_));
  status_.clear_active_time(); // Clear the result protobuf.

  // Test multiple consecutive active samples -- they should be coalesced
  // into a single active period.
  status_collector_.Simulate(test_states,
                             sizeof(test_states) / sizeof(IdleState));
  status_collector_.GetStatus(&status_);
  EXPECT_EQ(1, status_.active_time_size());
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
  status_collector_.Simulate(test_states,
                             sizeof(test_states) / sizeof(IdleState));
  status_collector_.GetStatus(&status_);
  EXPECT_EQ(3, status_.active_time_size());
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
  status_collector_.Simulate(test_states,
                             sizeof(test_states) / sizeof(IdleState));

  // Process the list a second time with a different collector.
  // It should be able to count the active periods found by the first
  // collector, because the results are stored in a pref.
  TestingDeviceStatusCollector second_collector(&prefs_,
                                                &statistics_provider_);
  second_collector.Simulate(test_states,
                            sizeof(test_states) / sizeof(IdleState));

  second_collector.GetStatus(&status_);
  EXPECT_EQ(4, status_.active_time_size());
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
  status_collector_.Simulate(test_states,
                             sizeof(test_states) / sizeof(IdleState));
  status_collector_.GetStatus(&status_);
  EXPECT_EQ(2, status_.active_time_size());

  EXPECT_EQ(3 * ActivePeriodMilliseconds(), GetActiveMilliseconds(status_));
}

TEST_F(DeviceStatusCollectorTest, MaxStoredPeriods) {
  IdleState test_states[] = {
    IDLE_STATE_ACTIVE,
    IDLE_STATE_IDLE
  };
  unsigned int max_periods = 10;

  status_collector_.set_max_stored_active_periods(max_periods);

  // Simulate 12 active periods.
  for (int i = 0; i < 12; i++) {
    status_collector_.Simulate(test_states,
                               sizeof(test_states) / sizeof(IdleState));
  }

  // Check that we don't exceed the max number of periods.
  status_collector_.GetStatus(&status_);
  EXPECT_EQ(static_cast<int>(max_periods), status_.active_time_size());
}

TEST_F(DeviceStatusCollectorTest, DevSwitchBootMode) {
  status_collector_.GetStatus(&status_);
  EXPECT_EQ(false, status_.has_boot_mode());

  EXPECT_CALL(statistics_provider_,
              GetMachineStatistic("devsw_boot", NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("(error)"), Return(true)));
  status_collector_.GetStatus(&status_);
  EXPECT_EQ(false, status_.has_boot_mode());

  EXPECT_CALL(statistics_provider_,
              GetMachineStatistic("devsw_boot", NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>(" "), Return(true)));
  status_collector_.GetStatus(&status_);
  EXPECT_EQ(false, status_.has_boot_mode());

  EXPECT_CALL(statistics_provider_,
              GetMachineStatistic("devsw_boot", NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("0"), Return(true)));
  status_collector_.GetStatus(&status_);
  EXPECT_EQ("Verified", status_.boot_mode());

  EXPECT_CALL(statistics_provider_,
              GetMachineStatistic("devsw_boot", NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>("1"), Return(true)));
  status_collector_.GetStatus(&status_);
  EXPECT_EQ("Dev", status_.boot_mode());
}

}  // namespace policy
