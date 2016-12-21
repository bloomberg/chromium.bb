// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector_impl.h"

#include <vector>

#include "base/macros.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
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

class TestUpgradeNotificationListener : public content::NotificationObserver {
 public:
  TestUpgradeNotificationListener() {
    registrar_.Add(this, chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
                   content::NotificationService::AllSources());
  }
  ~TestUpgradeNotificationListener() override {}

  const std::vector<int>& notifications_received() const {
    return notifications_received_;
  }

 private:
  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    notifications_received_.push_back(type);
  }

  // Registrar for listening to notifications.
  content::NotificationRegistrar registrar_;

  // Keeps track of the number and types of notifications that were received.
  std::vector<int> notifications_received_;

  DISALLOW_COPY_AND_ASSIGN(TestUpgradeNotificationListener);
};

TEST(UpgradeDetectorImplTest, VariationsChanges) {
  content::TestBrowserThreadBundle bundle;

  TestUpgradeNotificationListener notifications_listener;
  TestUpgradeDetectorImpl detector;
  EXPECT_FALSE(detector.notify_upgrade());
  EXPECT_TRUE(notifications_listener.notifications_received().empty());

  detector.OnExperimentChangesDetected(
      variations::VariationsService::Observer::BEST_EFFORT);
  EXPECT_FALSE(detector.notify_upgrade());
  EXPECT_TRUE(notifications_listener.notifications_received().empty());

  detector.NotifyOnUpgradeWithTimePassed(base::TimeDelta::FromDays(30));
  EXPECT_TRUE(detector.notify_upgrade());
  ASSERT_EQ(1U, notifications_listener.notifications_received().size());
  EXPECT_EQ(chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
            notifications_listener.notifications_received().front());
  EXPECT_EQ(0, detector.trigger_critical_update_call_count());

  // Execute tasks sent to FILE thread by |detector| referencing it
  // while it's still in scope.
  content::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
}

TEST(UpgradeDetectorImplTest, VariationsCriticalChanges) {
  content::TestBrowserThreadBundle bundle;

  TestUpgradeNotificationListener notifications_listener;
  TestUpgradeDetectorImpl detector;
  EXPECT_FALSE(detector.notify_upgrade());
  EXPECT_TRUE(notifications_listener.notifications_received().empty());

  detector.OnExperimentChangesDetected(
      variations::VariationsService::Observer::CRITICAL);
  EXPECT_FALSE(detector.notify_upgrade());
  EXPECT_TRUE(notifications_listener.notifications_received().empty());

  detector.NotifyOnUpgradeWithTimePassed(base::TimeDelta::FromDays(30));
  EXPECT_TRUE(detector.notify_upgrade());
  ASSERT_EQ(1U, notifications_listener.notifications_received().size());
  EXPECT_EQ(chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
            notifications_listener.notifications_received().front());
  EXPECT_EQ(1, detector.trigger_critical_update_call_count());

  // Execute tasks sent to FILE thread by |detector| referencing it
  // while it's still in scope.
  content::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
}
