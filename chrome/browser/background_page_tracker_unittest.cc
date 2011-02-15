// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/background_page_tracker.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/testing_browser_process_test.h"
#include "chrome/test/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class MockBackgroundPageTracker : public BackgroundPageTracker {
 public:
  MockBackgroundPageTracker() {
    BackgroundPageTracker::RegisterPrefs(&prefs_);
  }
  ~MockBackgroundPageTracker() {}
  // Overridden from BackgroundPageTracker to mock out functionality.
  virtual bool IsEnabled() { return true; }
  virtual PrefService* GetPrefService() { return &prefs_; }
 private:
  TestingPrefService prefs_;
};

class BackgroundPageTrackerTest : public TestingBrowserProcessTest {
};

TEST_F(BackgroundPageTrackerTest, Create) {
  MockBackgroundPageTracker tracker;
  EXPECT_EQ(0, tracker.GetBackgroundPageCount());
  EXPECT_EQ(0, tracker.GetUnacknowledgedBackgroundPageCount());
}

TEST_F(BackgroundPageTrackerTest, OnBackgroundPageLoaded) {
  MockBackgroundPageTracker tracker;
  EXPECT_EQ(0, tracker.GetBackgroundPageCount());
  EXPECT_EQ(0, tracker.GetUnacknowledgedBackgroundPageCount());
  std::string app1 = "app_id_1";
  std::string app2 = "app_id_2";
  tracker.OnBackgroundPageLoaded(app1);
  EXPECT_EQ(1, tracker.GetBackgroundPageCount());
  EXPECT_EQ(1, tracker.GetUnacknowledgedBackgroundPageCount());
  tracker.OnBackgroundPageLoaded(app1);
  EXPECT_EQ(1, tracker.GetBackgroundPageCount());
  EXPECT_EQ(1, tracker.GetUnacknowledgedBackgroundPageCount());
  tracker.OnBackgroundPageLoaded(app2);
  EXPECT_EQ(2, tracker.GetBackgroundPageCount());
  EXPECT_EQ(2, tracker.GetUnacknowledgedBackgroundPageCount());

  tracker.OnExtensionUnloaded(app1);
  EXPECT_EQ(1, tracker.GetBackgroundPageCount());
  EXPECT_EQ(1, tracker.GetUnacknowledgedBackgroundPageCount());

  tracker.OnExtensionUnloaded(app2);
  EXPECT_EQ(0, tracker.GetBackgroundPageCount());
  EXPECT_EQ(0, tracker.GetUnacknowledgedBackgroundPageCount());
}

TEST_F(BackgroundPageTrackerTest, AcknowledgeBackgroundPages) {
  MockBackgroundPageTracker tracker;
  EXPECT_EQ(0, tracker.GetBackgroundPageCount());
  EXPECT_EQ(0, tracker.GetUnacknowledgedBackgroundPageCount());
  std::string app1 = "app_id_1";
  tracker.OnBackgroundPageLoaded(app1);
  EXPECT_EQ(1, tracker.GetBackgroundPageCount());
  EXPECT_EQ(1, tracker.GetUnacknowledgedBackgroundPageCount());
  tracker.AcknowledgeBackgroundPages();
  EXPECT_EQ(1, tracker.GetBackgroundPageCount());
  EXPECT_EQ(0, tracker.GetUnacknowledgedBackgroundPageCount());
  tracker.OnBackgroundPageLoaded(app1);
  EXPECT_EQ(1, tracker.GetBackgroundPageCount());
  EXPECT_EQ(0, tracker.GetUnacknowledgedBackgroundPageCount());
}

class BadgeChangedNotificationCounter : public NotificationObserver {
 public:
  BadgeChangedNotificationCounter()
      : count_(0) {
    registrar_.Add(this,
                   NotificationType::BACKGROUND_PAGE_TRACKER_CHANGED,
                   NotificationService::AllSources());
  }
  // # notifications received.
  int count() { return count_; }
  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    EXPECT_EQ(type.value,
              NotificationType::BACKGROUND_PAGE_TRACKER_CHANGED);
    count_++;
  }
 private:
  int count_;
  NotificationRegistrar registrar_;
};

TEST_F(BackgroundPageTrackerTest, TestTrackerChangedNotifications) {
  MockBackgroundPageTracker tracker;
  BadgeChangedNotificationCounter counter;
  std::string app1 = "app_id_1";
  std::string app2 = "app_id_2";
  std::string app3 = "app_id_3";
  // New extension should generate notification
  tracker.OnBackgroundPageLoaded(app1);
  EXPECT_EQ(1, counter.count());
  // Same extension should not generate notification
  tracker.OnBackgroundPageLoaded(app1);
  EXPECT_EQ(1, counter.count());
  // New extension should generate notification
  tracker.OnBackgroundPageLoaded(app2);
  EXPECT_EQ(2, counter.count());
  // Acknowledging pages should generate notification.
  tracker.AcknowledgeBackgroundPages();
  EXPECT_EQ(3, counter.count());
  tracker.OnBackgroundPageLoaded(app1);
  EXPECT_EQ(3, counter.count());
  tracker.OnBackgroundPageLoaded(app3);
  EXPECT_EQ(4, counter.count());
}
