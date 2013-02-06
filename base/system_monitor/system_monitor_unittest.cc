// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system_monitor/system_monitor.h"

#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/test/mock_devices_changed_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class PowerTest : public SystemMonitor::PowerObserver {
 public:
  PowerTest()
      : power_state_changes_(0),
        suspends_(0),
        resumes_(0) {
  }

  // PowerObserver callbacks.
  virtual void OnPowerStateChange(bool on_battery_power) OVERRIDE {
    power_state_changes_++;
  }

  virtual void OnSuspend() OVERRIDE {
    suspends_++;
  }

  virtual void OnResume() OVERRIDE {
    resumes_++;
  }

  // Test status counts.
  int power_state_changes() { return power_state_changes_; }
  int suspends() { return suspends_; }
  int resumes() { return resumes_; }

 private:
  int power_state_changes_;  // Count of OnPowerStateChange notifications.
  int suspends_;  // Count of OnSuspend notifications.
  int resumes_;  // Count of OnResume notifications.
};

class SystemMonitorTest : public testing::Test {
 protected:
  SystemMonitorTest() {
#if defined(OS_MACOSX)
    // This needs to happen before SystemMonitor's ctor.
    SystemMonitor::AllocateSystemIOPorts();
#endif
    system_monitor_.reset(new SystemMonitor);
  }
  virtual ~SystemMonitorTest() {}

  MessageLoop message_loop_;
  scoped_ptr<SystemMonitor> system_monitor_;

  DISALLOW_COPY_AND_ASSIGN(SystemMonitorTest);
};

TEST_F(SystemMonitorTest, PowerNotifications) {
  const int kObservers = 5;

  PowerTest test[kObservers];
  for (int index = 0; index < kObservers; ++index)
    system_monitor_->AddPowerObserver(&test[index]);

  // Send a bunch of power changes.  Since the battery power hasn't
  // actually changed, we shouldn't get notifications.
  for (int index = 0; index < 5; index++) {
    system_monitor_->ProcessPowerMessage(SystemMonitor::POWER_STATE_EVENT);
    EXPECT_EQ(test[0].power_state_changes(), 0);
  }

  // Sending resume when not suspended should have no effect.
  system_monitor_->ProcessPowerMessage(SystemMonitor::RESUME_EVENT);
  RunLoop().RunUntilIdle();
  EXPECT_EQ(test[0].resumes(), 0);

  // Pretend we suspended.
  system_monitor_->ProcessPowerMessage(SystemMonitor::SUSPEND_EVENT);
  RunLoop().RunUntilIdle();
  EXPECT_EQ(test[0].suspends(), 1);

  // Send a second suspend notification.  This should be suppressed.
  system_monitor_->ProcessPowerMessage(SystemMonitor::SUSPEND_EVENT);
  RunLoop().RunUntilIdle();
  EXPECT_EQ(test[0].suspends(), 1);

  // Pretend we were awakened.
  system_monitor_->ProcessPowerMessage(SystemMonitor::RESUME_EVENT);
  RunLoop().RunUntilIdle();
  EXPECT_EQ(test[0].resumes(), 1);

  // Send a duplicate resume notification.  This should be suppressed.
  system_monitor_->ProcessPowerMessage(SystemMonitor::RESUME_EVENT);
  RunLoop().RunUntilIdle();
  EXPECT_EQ(test[0].resumes(), 1);
}

TEST_F(SystemMonitorTest, DeviceChangeNotifications) {
  const int kObservers = 5;

  testing::Sequence mock_sequencer[kObservers];
  MockDevicesChangedObserver observers[kObservers];
  for (int index = 0; index < kObservers; ++index) {
    system_monitor_->AddDevicesChangedObserver(&observers[index]);

    EXPECT_CALL(observers[index],
                OnDevicesChanged(SystemMonitor::DEVTYPE_UNKNOWN))
        .Times(3)
        .InSequence(mock_sequencer[index]);
  }

  system_monitor_->ProcessDevicesChanged(SystemMonitor::DEVTYPE_UNKNOWN);
  RunLoop().RunUntilIdle();

  system_monitor_->ProcessDevicesChanged(SystemMonitor::DEVTYPE_UNKNOWN);
  system_monitor_->ProcessDevicesChanged(SystemMonitor::DEVTYPE_UNKNOWN);
  RunLoop().RunUntilIdle();
}

}  // namespace

}  // namespace base
