// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector_impl.h"

#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/upgrade_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestUpgradeDetectorImpl : public UpgradeDetectorImpl {
 public:
  explicit TestUpgradeDetectorImpl(const base::TickClock* tick_clock)
      : UpgradeDetectorImpl(tick_clock) {}
  ~TestUpgradeDetectorImpl() override = default;

  // Exposed for testing.
  using UpgradeDetectorImpl::UPGRADE_AVAILABLE_REGULAR;
  using UpgradeDetectorImpl::UpgradeDetected;
  using UpgradeDetectorImpl::OnExperimentChangesDetected;
  using UpgradeDetectorImpl::NotifyOnUpgradeWithTimePassed;
  using UpgradeDetectorImpl::GetThresholdForLevel;

  // UpgradeDetector:
  void TriggerCriticalUpdate() override {
    ++trigger_critical_update_call_count_;
  }

  int trigger_critical_update_call_count() const {
    return trigger_critical_update_call_count_;
  }

 private:
  // How many times TriggerCriticalUpdate() has been called. Expected to either
  // be 0 or 1.
  int trigger_critical_update_call_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestUpgradeDetectorImpl);
};

class TestUpgradeNotificationListener : public UpgradeObserver {
 public:
  explicit TestUpgradeNotificationListener(UpgradeDetector* detector)
      : notifications_count_(0), detector_(detector) {
    DCHECK(detector_);
    detector_->AddObserver(this);
  }
  ~TestUpgradeNotificationListener() override {
    detector_->RemoveObserver(this);
  }

  int notification_count() const { return notifications_count_; }

 private:
  // UpgradeObserver:
  void OnUpgradeRecommended() override { ++notifications_count_; }

  // The number of upgrade recommended notifications received.
  int notifications_count_;

  UpgradeDetector* detector_;

  DISALLOW_COPY_AND_ASSIGN(TestUpgradeNotificationListener);
};

class MockUpgradeObserver : public UpgradeObserver {
 public:
  explicit MockUpgradeObserver(UpgradeDetector* upgrade_detector)
      : upgrade_detector_(upgrade_detector) {
    upgrade_detector_->AddObserver(this);
  }
  ~MockUpgradeObserver() override { upgrade_detector_->RemoveObserver(this); }
  MOCK_METHOD0(OnUpdateOverCellularAvailable, void());
  MOCK_METHOD0(OnUpdateOverCellularOneTimePermissionGranted, void());
  MOCK_METHOD0(OnUpgradeRecommended, void());
  MOCK_METHOD0(OnCriticalUpgradeInstalled, void());
  MOCK_METHOD0(OnOutdatedInstall, void());
  MOCK_METHOD0(OnOutdatedInstallNoAutoUpdate, void());

 private:
  UpgradeDetector* const upgrade_detector_;
  DISALLOW_COPY_AND_ASSIGN(MockUpgradeObserver);
};

}  // namespace

class UpgradeDetectorImplTest : public ::testing::Test {
 protected:
  UpgradeDetectorImplTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        scoped_local_state_(TestingBrowserProcess::GetGlobal()) {}

  const base::TickClock* GetMockTickClock() {
    return scoped_task_environment_.GetMockTickClock();
  }

  // Sets the browser.relaunch_notification_period preference in Local State to
  // |value|.
  void SetNotificationPeriodPref(base::TimeDelta value) {
    if (value.is_zero()) {
      scoped_local_state_.Get()->RemoveManagedPref(
          prefs::kRelaunchNotificationPeriod);
    } else {
      scoped_local_state_.Get()->SetManagedPref(
          prefs::kRelaunchNotificationPeriod,
          std::make_unique<base::Value>(
              base::saturated_cast<int>(value.InMilliseconds())));
    }
  }

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

  // Fast-forwards virtual time by |delta|.
  void FastForwardBy(base::TimeDelta delta) {
    scoped_task_environment_.FastForwardBy(delta);
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ScopedTestingLocalState scoped_local_state_;

  DISALLOW_COPY_AND_ASSIGN(UpgradeDetectorImplTest);
};

TEST_F(UpgradeDetectorImplTest, VariationsChanges) {
  TestUpgradeDetectorImpl detector(GetMockTickClock());
  TestUpgradeNotificationListener notifications_listener(&detector);
  EXPECT_FALSE(detector.notify_upgrade());
  EXPECT_EQ(0, notifications_listener.notification_count());

  detector.OnExperimentChangesDetected(
      variations::VariationsService::Observer::BEST_EFFORT);
  EXPECT_FALSE(detector.notify_upgrade());
  EXPECT_EQ(0, notifications_listener.notification_count());

  detector.NotifyOnUpgradeWithTimePassed(base::TimeDelta::FromDays(30));
  EXPECT_TRUE(detector.notify_upgrade());
  EXPECT_EQ(1, notifications_listener.notification_count());
  EXPECT_EQ(0, detector.trigger_critical_update_call_count());

  // Execute tasks posted by |detector| referencing it while it's still in
  // scope.
  RunUntilIdle();
}

TEST_F(UpgradeDetectorImplTest, VariationsCriticalChanges) {
  TestUpgradeDetectorImpl detector(GetMockTickClock());
  TestUpgradeNotificationListener notifications_listener(&detector);
  EXPECT_FALSE(detector.notify_upgrade());
  EXPECT_EQ(0, notifications_listener.notification_count());

  detector.OnExperimentChangesDetected(
      variations::VariationsService::Observer::CRITICAL);
  EXPECT_FALSE(detector.notify_upgrade());
  EXPECT_EQ(0, notifications_listener.notification_count());

  detector.NotifyOnUpgradeWithTimePassed(base::TimeDelta::FromDays(30));
  EXPECT_TRUE(detector.notify_upgrade());
  EXPECT_EQ(1, notifications_listener.notification_count());
  EXPECT_EQ(1, detector.trigger_critical_update_call_count());

  // Execute tasks posted by |detector| referencing it while it's still in
  // scope.
  RunUntilIdle();
}

TEST_F(UpgradeDetectorImplTest, TestPeriodChanges) {
  // Fast forward a little bit to get away from zero ticks, which has special
  // meaning in the detector.
  FastForwardBy(base::TimeDelta::FromHours(1));

  TestUpgradeDetectorImpl upgrade_detector(GetMockTickClock());
  ::testing::StrictMock<MockUpgradeObserver> mock_observer(&upgrade_detector);

  // Changing the period when no upgrade has been detected updates the
  // thresholds and nothing else.
  SetNotificationPeriodPref(base::TimeDelta::FromHours(3));

  EXPECT_EQ(upgrade_detector.GetThresholdForLevel(
                UpgradeDetector::UPGRADE_ANNOYANCE_HIGH),
            base::TimeDelta::FromHours(3));
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Back to default.
  SetNotificationPeriodPref(base::TimeDelta());
  EXPECT_EQ(upgrade_detector.GetThresholdForLevel(
                UpgradeDetector::UPGRADE_ANNOYANCE_HIGH),
            base::TimeDelta::FromDays(7));
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Pretend that an upgrade was just detected now.
  upgrade_detector.UpgradeDetected(
      TestUpgradeDetectorImpl::UPGRADE_AVAILABLE_REGULAR);
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Fast forward an amount that is still in the "don't annoy me" period at the
  // default period.
  FastForwardBy(base::TimeDelta::FromHours(1));
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Drop the period so that the current time is in the "low" annoyance level.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta::FromHours(3));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_LOW);

  // Bring it back up.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta());
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  // Fast forward an amount that is still in the "don't annoy me" period at the
  // default period.
  FastForwardBy(base::TimeDelta::FromHours(1));
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Drop the period so that the current time is in the "elevated" level.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta::FromHours(3));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);

  // Bring it back up.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta());
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  // Fast forward an amount that is still in the "don't annoy me" period at the
  // default period.
  FastForwardBy(base::TimeDelta::FromHours(1));
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Drop the period so that the current time is in the "high" level.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta::FromHours(3));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);

  // Bring it back up.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta());
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  // Fast forward an amount that is still in the "don't annoy me" period at the
  // default period.
  FastForwardBy(base::TimeDelta::FromHours(1));
  ::testing::Mock::VerifyAndClear(&mock_observer);

  // Drop the period so that the current time is deep in the "high" level.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta::FromHours(3));
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);

  // Bring it back up.
  EXPECT_CALL(mock_observer, OnUpgradeRecommended());
  SetNotificationPeriodPref(base::TimeDelta());
  ::testing::Mock::VerifyAndClear(&mock_observer);
  EXPECT_EQ(upgrade_detector.upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
}
