// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"

#include "base/threading/platform_thread.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/test/fake_arc_session.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class ArcBootPhaseMonitorBridgeTest : public testing::Test {
 public:
  ArcBootPhaseMonitorBridgeTest() = default;
  ~ArcBootPhaseMonitorBridgeTest() override = default;

  void SetUp() override {
    chromeos::DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        base::MakeUnique<chromeos::FakeSessionManagerClient>());
    chromeos::DBusThreadManager::Initialize();

    record_uma_counter_ = 0;
    testing_profile_ = base::MakeUnique<TestingProfile>();
    arc_session_manager_ = base::MakeUnique<ArcSessionManager>(
        base::MakeUnique<ArcSessionRunner>(base::Bind(FakeArcSession::Create)));
    bridge_service_ = base::MakeUnique<ArcBridgeService>();

    boot_phase_monitor_bridge_ = base::MakeUnique<ArcBootPhaseMonitorBridge>(
        testing_profile_.get(), bridge_service_.get());
    boot_phase_monitor_bridge_->set_first_app_launch_delay_recorder_for_testing(
        base::BindRepeating(&ArcBootPhaseMonitorBridgeTest::RecordUMA,
                            base::Unretained(this)));
  }

  void TearDown() override {
    boot_phase_monitor_bridge_->Shutdown();
    boot_phase_monitor_bridge_.reset();

    bridge_service_.reset();
    arc_session_manager_.reset();
    testing_profile_.reset();
  }

 protected:
  ArcSessionManager* arc_session_manager() const {
    return arc_session_manager_.get();
  }
  ArcBootPhaseMonitorBridge* boot_phase_monitor_bridge() const {
    return boot_phase_monitor_bridge_.get();
  }
  size_t record_uma_counter() const { return record_uma_counter_; }
  base::TimeDelta last_time_delta() const { return last_time_delta_; }

 private:
  void RecordUMA(base::TimeDelta delta) {
    last_time_delta_ = delta;
    ++record_uma_counter_;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<ArcBridgeService> bridge_service_;
  std::unique_ptr<ArcBootPhaseMonitorBridge> boot_phase_monitor_bridge_;

  size_t record_uma_counter_;
  base::TimeDelta last_time_delta_;

  DISALLOW_COPY_AND_ASSIGN(ArcBootPhaseMonitorBridgeTest);
};

// Tests that ArcBootPhaseMonitorBridge can be constructed and destructed.
TEST_F(ArcBootPhaseMonitorBridgeTest, TestConstructDestruct) {}

// Tests that the throttle is created on BootCompleted.
TEST_F(ArcBootPhaseMonitorBridgeTest, TestThrottleCreation) {
  // Tell |arc_session_manager_| that this is not opt-in boot.
  arc_session_manager()->set_directly_started_for_testing(true);

  // Initially, |throttle_| is null.
  EXPECT_EQ(nullptr, boot_phase_monitor_bridge()->throttle_for_testing());
  // OnBootCompleted() creates it.
  boot_phase_monitor_bridge()->OnBootCompleted();
  EXPECT_NE(nullptr, boot_phase_monitor_bridge()->throttle_for_testing());
  // ..but it's removed when the session stops.
  boot_phase_monitor_bridge()->OnArcSessionStopped(ArcStopReason::SHUTDOWN);
  EXPECT_EQ(nullptr, boot_phase_monitor_bridge()->throttle_for_testing());
}

// Tests the same but with OnSessionRestarting().
TEST_F(ArcBootPhaseMonitorBridgeTest, TestThrottleCreation_Restart) {
  // Tell |arc_session_manager_| that this is not opt-in boot.
  arc_session_manager()->set_directly_started_for_testing(true);

  EXPECT_EQ(nullptr, boot_phase_monitor_bridge()->throttle_for_testing());
  boot_phase_monitor_bridge()->OnBootCompleted();
  EXPECT_NE(nullptr, boot_phase_monitor_bridge()->throttle_for_testing());
  // Call OnArcSessionRestarting() instead, and confirm that |throttle_| is
  // gone.
  boot_phase_monitor_bridge()->OnArcSessionRestarting();
  EXPECT_EQ(nullptr, boot_phase_monitor_bridge()->throttle_for_testing());
  // Also make sure that |throttle| is created again once restarting id done.
  boot_phase_monitor_bridge()->OnBootCompleted();
  EXPECT_NE(nullptr, boot_phase_monitor_bridge()->throttle_for_testing());
}

// Tests that the throttle is created on ArcInitialStart when opting in.
TEST_F(ArcBootPhaseMonitorBridgeTest, TestThrottleCreation_OptIn) {
  // Tell |arc_session_manager_| that this *is* opt-in boot.
  arc_session_manager()->set_directly_started_for_testing(false);

  // Initially, |throttle_| is null.
  EXPECT_EQ(nullptr, boot_phase_monitor_bridge()->throttle_for_testing());
  // OnArcInitialStart(), which is called when the user accepts ToS, creates
  // |throttle_|.
  boot_phase_monitor_bridge()->OnArcInitialStart();
  EXPECT_NE(nullptr, boot_phase_monitor_bridge()->throttle_for_testing());
  // ..and OnBootCompleted() does not delete it.
  boot_phase_monitor_bridge()->OnBootCompleted();
  EXPECT_NE(nullptr, boot_phase_monitor_bridge()->throttle_for_testing());
  // ..but it's removed when the session stops.
  boot_phase_monitor_bridge()->OnArcSessionStopped(ArcStopReason::SHUTDOWN);
  EXPECT_EQ(nullptr, boot_phase_monitor_bridge()->throttle_for_testing());
}

// Tests that the UMA recording function is never called unless
// RecordFirstAppLaunchDelayUMA is called.
TEST_F(ArcBootPhaseMonitorBridgeTest, TestRecordUMA_None) {
  EXPECT_EQ(0U, record_uma_counter());
  boot_phase_monitor_bridge()->OnBootCompleted();
  EXPECT_EQ(0U, record_uma_counter());
  boot_phase_monitor_bridge()->OnArcSessionStopped(ArcStopReason::SHUTDOWN);
  EXPECT_EQ(0U, record_uma_counter());
}

// Tests that RecordFirstAppLaunchDelayUMA() actually calls the UMA recording
// function (but only after OnBootCompleted.)
TEST_F(ArcBootPhaseMonitorBridgeTest, TestRecordUMA_AppLaunchBeforeBoot) {
  EXPECT_EQ(0U, record_uma_counter());
  // Calling RecordFirstAppLaunchDelayUMA() before boot shouldn't immediately
  // record UMA.
  boot_phase_monitor_bridge()->RecordFirstAppLaunchDelayUMAForTesting();
  EXPECT_EQ(0U, record_uma_counter());
  // Sleep for 1ms just to make sure 0 won't be passed to RecordUMA().
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1));
  // UMA recording should be done on BootCompleted.
  boot_phase_monitor_bridge()->OnBootCompleted();
  EXPECT_EQ(1U, record_uma_counter());
  // In this case, |delta| passed to the UMA recording function should be >0.
  EXPECT_LT(base::TimeDelta(), last_time_delta());
}

// Tests the same with calling RecordFirstAppLaunchDelayUMA() after boot.
TEST_F(ArcBootPhaseMonitorBridgeTest, TestRecordUMA_AppLaunchAfterBoot) {
  EXPECT_EQ(0U, record_uma_counter());
  boot_phase_monitor_bridge()->OnBootCompleted();
  EXPECT_EQ(0U, record_uma_counter());
  // Calling RecordFirstAppLaunchDelayUMA() after boot should immediately record
  // UMA.
  boot_phase_monitor_bridge()->RecordFirstAppLaunchDelayUMAForTesting();
  EXPECT_EQ(1U, record_uma_counter());
  // In this case, |delta| passed to the UMA recording function should be 0.
  EXPECT_TRUE(last_time_delta().is_zero());
}

// Tests the same with calling RecordFirstAppLaunchDelayUMA() twice.
TEST_F(ArcBootPhaseMonitorBridgeTest,
       TestRecordUMA_AppLaunchesBeforeAndAfterBoot) {
  EXPECT_EQ(0U, record_uma_counter());
  boot_phase_monitor_bridge()->RecordFirstAppLaunchDelayUMAForTesting();
  EXPECT_EQ(0U, record_uma_counter());
  boot_phase_monitor_bridge()->OnBootCompleted();
  EXPECT_EQ(1U, record_uma_counter());
  EXPECT_LT(base::TimeDelta(), last_time_delta());
  // Call the record function again and check that the counter is not changed.
  boot_phase_monitor_bridge()->RecordFirstAppLaunchDelayUMAForTesting();
  EXPECT_EQ(1U, record_uma_counter());
}

// Tests the same with calling RecordFirstAppLaunchDelayUMA() twice after boot.
TEST_F(ArcBootPhaseMonitorBridgeTest, TestRecordUMA_AppLaunchesAfterBoot) {
  EXPECT_EQ(0U, record_uma_counter());
  boot_phase_monitor_bridge()->OnBootCompleted();
  EXPECT_EQ(0U, record_uma_counter());
  boot_phase_monitor_bridge()->RecordFirstAppLaunchDelayUMAForTesting();
  EXPECT_EQ(1U, record_uma_counter());
  EXPECT_TRUE(last_time_delta().is_zero());
  // Call the record function again and check that the counter is not changed.
  boot_phase_monitor_bridge()->RecordFirstAppLaunchDelayUMAForTesting();
  EXPECT_EQ(1U, record_uma_counter());
}

}  // namespace

}  // namespace arc
