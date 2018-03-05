// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_notification_controller.h"

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A delegate interface for handling the actions taken by the controller.
class ControllerDelegate {
 public:
  virtual ~ControllerDelegate() = default;
  virtual void ShowRelaunchRecommendedBubble() = 0;
  virtual void ShowRelaunchRequiredDialog() = 0;
  virtual void CloseWidget() = 0;
  virtual void OnRelaunchDeadlineExpired() = 0;

 protected:
  ControllerDelegate() = default;
};

// A fake controller that asks a delegate to do work.
class FakeRelaunchNotificationController
    : public RelaunchNotificationController {
 public:
  FakeRelaunchNotificationController(UpgradeDetector* upgrade_detector,
                                     ControllerDelegate* delegate)
      : RelaunchNotificationController(upgrade_detector), delegate_(delegate) {}

 private:
  void ShowRelaunchRecommendedBubble() override {
    delegate_->ShowRelaunchRecommendedBubble();
  }

  void ShowRelaunchRequiredDialog() override {
    delegate_->ShowRelaunchRequiredDialog();
  }

  void CloseWidget() override { delegate_->CloseWidget(); }

  void OnRelaunchDeadlineExpired() override {
    delegate_->OnRelaunchDeadlineExpired();
  }

  ControllerDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(FakeRelaunchNotificationController);
};

// A mock delegate for testing.
class MockControllerDelegate : public ControllerDelegate {
 public:
  MOCK_METHOD0(ShowRelaunchRecommendedBubble, void());
  MOCK_METHOD0(ShowRelaunchRequiredDialog, void());
  MOCK_METHOD0(CloseWidget, void());
  MOCK_METHOD0(OnRelaunchDeadlineExpired, void());
};

// A fake UpgradeDetector.
class FakeUpgradeDetector : public UpgradeDetector {
 public:
  FakeUpgradeDetector() = default;

  base::TimeDelta GetHighAnnoyanceLevelDelta() override {
    return base::TimeDelta::FromMinutes(1);
  }

  base::TimeTicks GetHighAnnoyanceDeadline() override {
    return base::TimeTicks::Now() - base::TimeDelta::FromDays(1);  // The past.
  }

  // Sets the annoyance level to |level| and broadcasts the change to all
  // observers.
  void BroadcastLevelChange(UpgradeNotificationAnnoyanceLevel level) {
    set_upgrade_notification_stage(level);
    NotifyUpgrade();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeUpgradeDetector);
};

}  // namespace

// A test harness that provides facilities for manipulating the relaunch
// notification policy setting and for broadcasting upgrade notifications.
class RelaunchNotificationControllerTest : public ::testing::Test {
 protected:
  RelaunchNotificationControllerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
            base::test::ScopedTaskEnvironment::ExecutionMode::QUEUED),
        scoped_local_state_(TestingBrowserProcess::GetGlobal()) {}
  UpgradeDetector* upgrade_detector() { return &upgrade_detector_; }
  FakeUpgradeDetector& fake_upgrade_detector() { return upgrade_detector_; }

  // Sets the browser.relaunch_notification preference in Local State to
  // |value|.
  void SetNotificationPref(int value) {
    scoped_local_state_.Get()->SetManagedPref(
        prefs::kRelaunchNotification, std::make_unique<base::Value>(value));
  }

  // Fast-forwards virtual time by |delta|.
  void FastForwardBy(base::TimeDelta delta) {
    scoped_task_environment_.FastForwardBy(delta);
  }

  // Runs tasks until the queues are empty.
  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ScopedTestingLocalState scoped_local_state_;
  FakeUpgradeDetector upgrade_detector_;

  DISALLOW_COPY_AND_ASSIGN(RelaunchNotificationControllerTest);
};

TEST_F(RelaunchNotificationControllerTest, CreateDestroy) {
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(upgrade_detector(),
                                                &mock_controller_delegate);
}

// Without the browser.relaunch_notification preference set, the controller
// should not be observing the UpgradeDetector, and should therefore never
// attempt to show any notifications.
TEST_F(RelaunchNotificationControllerTest, PolicyUnset) {
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(upgrade_detector(),
                                                &mock_controller_delegate);

  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
}

// With the browser.relaunch_notification preference set to 1, the controller
// should be observing the UpgradeDetector and should show "Requested"
// notifications on each level change.
TEST_F(RelaunchNotificationControllerTest, RecommendedByPolicy) {
  SetNotificationPref(1);
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(upgrade_detector(),
                                                &mock_controller_delegate);

  // Nothing shown if the level is broadcast at NONE.
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  // Show for each level change, but not for repeat notifications.
  EXPECT_CALL(mock_controller_delegate, ShowRelaunchRecommendedBubble());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, ShowRelaunchRecommendedBubble());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, ShowRelaunchRecommendedBubble());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  // The timer should be running to reshow at the detector's delta (1m).
  EXPECT_CALL(mock_controller_delegate, ShowRelaunchRecommendedBubble());
  FastForwardBy(base::TimeDelta::FromMinutes(1));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);
  EXPECT_CALL(mock_controller_delegate, ShowRelaunchRecommendedBubble());
  FastForwardBy(base::TimeDelta::FromMinutes(1));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  // Drop back to elevated to stop the reshows and ensure there are none.
  EXPECT_CALL(mock_controller_delegate, ShowRelaunchRecommendedBubble());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);
  FastForwardBy(base::TimeDelta::FromMinutes(1));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  // And closed if the level drops back to none.
  EXPECT_CALL(mock_controller_delegate, CloseWidget());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);
}

// With the browser.relaunch_notification preference set to 2, the controller
// should be observing the UpgradeDetector and should show "Required"
// notifications on each level change.
TEST_F(RelaunchNotificationControllerTest, RequiredByPolicy) {
  SetNotificationPref(2);
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(upgrade_detector(),
                                                &mock_controller_delegate);

  // Nothing shown if the level is broadcast at NONE.
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  // Show for each level change, but not for repeat notifications.
  EXPECT_CALL(mock_controller_delegate, ShowRelaunchRequiredDialog());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, ShowRelaunchRequiredDialog());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, ShowRelaunchRequiredDialog());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, ShowRelaunchRequiredDialog());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  // And closed if the level drops back to none.
  EXPECT_CALL(mock_controller_delegate, CloseWidget());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);
}

// Flipping the policy should have no effect when at level NONE
TEST_F(RelaunchNotificationControllerTest, PolicyChangesNoUpgrade) {
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(upgrade_detector(),
                                                &mock_controller_delegate);

  SetNotificationPref(1);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  SetNotificationPref(2);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  SetNotificationPref(3);  // Bogus value!
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  SetNotificationPref(0);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);
}

// Policy changes at an elevated level should show the appropriate notification.
TEST_F(RelaunchNotificationControllerTest, PolicyChangesWithUpgrade) {
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(upgrade_detector(),
                                                &mock_controller_delegate);

  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, ShowRelaunchRecommendedBubble());
  SetNotificationPref(1);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, CloseWidget());
  EXPECT_CALL(mock_controller_delegate, ShowRelaunchRequiredDialog());
  SetNotificationPref(2);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  EXPECT_CALL(mock_controller_delegate, CloseWidget());
  SetNotificationPref(0);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);
}

// Relaunch is forced when the deadline is reached.
TEST_F(RelaunchNotificationControllerTest, RequiredDeadlineReached) {
  SetNotificationPref(2);
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(upgrade_detector(),
                                                &mock_controller_delegate);

  // As in the RequiredByPolicy test, the dialog should be shown.
  EXPECT_CALL(mock_controller_delegate, ShowRelaunchRequiredDialog());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  // And the relaunch should be forced after the deadline passes.
  EXPECT_CALL(mock_controller_delegate, OnRelaunchDeadlineExpired());
  FastForwardBy(base::TimeDelta::FromMinutes(5));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);
}

// No forced relaunch if the dialog is closed.
TEST_F(RelaunchNotificationControllerTest, RequiredDeadlineReachedNoPolicy) {
  SetNotificationPref(2);
  ::testing::StrictMock<MockControllerDelegate> mock_controller_delegate;

  FakeRelaunchNotificationController controller(upgrade_detector(),
                                                &mock_controller_delegate);

  // As in the RequiredByPolicy test, the dialog should be shown.
  EXPECT_CALL(mock_controller_delegate, ShowRelaunchRequiredDialog());
  fake_upgrade_detector().BroadcastLevelChange(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  // And then closed if the policy is cleared.
  EXPECT_CALL(mock_controller_delegate, CloseWidget());
  SetNotificationPref(0);
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);

  // And no relaunch should take place.
  FastForwardBy(base::TimeDelta::FromMinutes(5));
  RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_controller_delegate);
}
