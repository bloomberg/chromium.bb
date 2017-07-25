// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector_impl.h"

#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/upgrade_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestUpgradeDetectorImpl : public UpgradeDetectorImpl {
 public:
  TestUpgradeDetectorImpl() = default;
  ~TestUpgradeDetectorImpl() override = default;

  // Methods exposed for testing.
  using UpgradeDetectorImpl::OnExperimentChangesDetected;
  using UpgradeDetectorImpl::NotifyOnUpgradeWithTimePassed;

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

}  // namespace

TEST(UpgradeDetectorImplTest, VariationsChanges) {
  base::test::ScopedTaskEnvironment task_environment;

  TestUpgradeDetectorImpl detector;
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
  task_environment.RunUntilIdle();
}

TEST(UpgradeDetectorImplTest, VariationsCriticalChanges) {
  base::test::ScopedTaskEnvironment task_environment;

  TestUpgradeDetectorImpl detector;
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
  task_environment.RunUntilIdle();
}
