// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector_impl.h"

#include <vector>

#include "base/macros.h"
#include "chrome/browser/upgrade_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestUpgradeDetectorImpl : public UpgradeDetectorImpl {
 public:
  TestUpgradeDetectorImpl() : trigger_critical_update_call_count_(0) {}
  ~TestUpgradeDetectorImpl() override {}

  // Methods exposed for testing.
  using UpgradeDetectorImpl::OnExperimentChangesDetected;
  using UpgradeDetectorImpl::NotifyOnUpgradeWithTimePassed;

  // UpgradeDetector:
  void TriggerCriticalUpdate() override {
    trigger_critical_update_call_count_++;
  }

  int trigger_critical_update_call_count() const {
    return trigger_critical_update_call_count_;
  }

 private:
  // How many times TriggerCriticalUpdate() has been called. Expected to either
  // be 0 or 1.
  int trigger_critical_update_call_count_;

  DISALLOW_COPY_AND_ASSIGN(TestUpgradeDetectorImpl);
};

class TestUpgradeNotificationListener : public UpgradeObserver {
 public:
  explicit TestUpgradeNotificationListener(UpgradeDetector* detector)
      : notifications_count_(0), detector_(detector) {
    detector_->AddObserver(this);
  }
  ~TestUpgradeNotificationListener() override {
    if (detector_)
      detector_->RemoveObserver(this);
  }

  int get_notifications_count() const { return notifications_count_; }

 private:
  // UpgradeObserver implementation.
  void OnUpgradeRecommended() override { notifications_count_ += 1; }

  // Keeps track of the number of upgrade recommended notifications that were
  // received.
  int notifications_count_;

  UpgradeDetector* detector_;

  DISALLOW_COPY_AND_ASSIGN(TestUpgradeNotificationListener);
};

TEST(UpgradeDetectorImplTest, VariationsChanges) {
  content::TestBrowserThreadBundle bundle;

  TestUpgradeDetectorImpl detector;
  TestUpgradeNotificationListener notifications_listener(&detector);
  EXPECT_FALSE(detector.notify_upgrade());
  EXPECT_EQ(0, notifications_listener.get_notifications_count());

  detector.OnExperimentChangesDetected(
      variations::VariationsService::Observer::BEST_EFFORT);
  EXPECT_FALSE(detector.notify_upgrade());
  EXPECT_EQ(0, notifications_listener.get_notifications_count());

  detector.NotifyOnUpgradeWithTimePassed(base::TimeDelta::FromDays(30));
  EXPECT_TRUE(detector.notify_upgrade());
  EXPECT_EQ(1, notifications_listener.get_notifications_count());
  EXPECT_EQ(0, detector.trigger_critical_update_call_count());

  // Execute tasks sent to FILE thread by |detector| referencing it
  // while it's still in scope.
  content::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
}

TEST(UpgradeDetectorImplTest, VariationsCriticalChanges) {
  content::TestBrowserThreadBundle bundle;

  TestUpgradeDetectorImpl detector;
  TestUpgradeNotificationListener notifications_listener(&detector);
  EXPECT_FALSE(detector.notify_upgrade());
  EXPECT_EQ(0, notifications_listener.get_notifications_count());

  detector.OnExperimentChangesDetected(
      variations::VariationsService::Observer::CRITICAL);
  EXPECT_FALSE(detector.notify_upgrade());
  EXPECT_EQ(0, notifications_listener.get_notifications_count());

  detector.NotifyOnUpgradeWithTimePassed(base::TimeDelta::FromDays(30));
  EXPECT_TRUE(detector.notify_upgrade());
  EXPECT_EQ(1, notifications_listener.get_notifications_count());
  EXPECT_EQ(1, detector.trigger_critical_update_call_count());

  // Execute tasks sent to FILE thread by |detector| referencing it
  // while it's still in scope.
  content::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
}
