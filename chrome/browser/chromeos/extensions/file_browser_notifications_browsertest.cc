// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_browser_notifications.h"

#include <gtest/gtest.h>
#include <string>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

namespace chromeos {

class FileBrowserNotificationsTest : public InProcessBrowserTest {
 public:
  FileBrowserNotificationsTest() {}

  virtual void CleanUpOnMainThread() OVERRIDE {
    notifications_.reset();
  }

 protected:
  // This must be initialized late in test startup.
  void InitNotifications() {
    Profile* profile = browser()->profile();
    notifications_.reset(new FileBrowserNotifications(profile));
  }

  bool FindNotification(const std::string& id) {
    return notifications_->HasNotificationForTest(id);
  }

  scoped_ptr<FileBrowserNotifications> notifications_;
};

IN_PROC_BROWSER_TEST_F(FileBrowserNotificationsTest, TestBasic) {
  InitNotifications();
  // Showing a notification adds a new notification.
  notifications_->ShowNotification(FileBrowserNotifications::DEVICE, "path");
  EXPECT_EQ(1u, notifications_->GetNotificationCountForTest());
  EXPECT_TRUE(FindNotification("Dpath"));

  // Updating the same notification maintains the same count.
  notifications_->ShowNotification(FileBrowserNotifications::DEVICE, "path");
  EXPECT_EQ(1u, notifications_->GetNotificationCountForTest());
  EXPECT_TRUE(FindNotification("Dpath"));

  // A new notification increases the count.
  notifications_->ShowNotification(FileBrowserNotifications::DEVICE_FAIL,
                                   "path");
  EXPECT_EQ(2u, notifications_->GetNotificationCountForTest());
  EXPECT_TRUE(FindNotification("DFpath"));
  EXPECT_TRUE(FindNotification("Dpath"));

  // Hiding a notification removes it from our data.
  notifications_->HideNotification(FileBrowserNotifications::DEVICE_FAIL,
                                   "path");
  EXPECT_EQ(1u, notifications_->GetNotificationCountForTest());
  EXPECT_FALSE(FindNotification("DFpath"));
  EXPECT_TRUE(FindNotification("Dpath"));
};

// Note: Delayed tests use a delay time of 0 so that tasks wille execute
// when RunAllPendingInMessageLoop() is called.
IN_PROC_BROWSER_TEST_F(FileBrowserNotificationsTest, ShowDelayedTest) {
  InitNotifications();
  // Adding a delayed notification does not create a notification.
  notifications_->ShowNotificationDelayed(FileBrowserNotifications::DEVICE,
                                          "path",
                                          base::TimeDelta::FromSeconds(0));
  EXPECT_EQ(0u, notifications_->GetNotificationCountForTest());
  EXPECT_FALSE(FindNotification("Dpath"));

  // Running the message loop should create the notification.
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(1u, notifications_->GetNotificationCountForTest());
  EXPECT_TRUE(FindNotification("Dpath"));

  // Showing a notification both immediately and delayed results in one
  // additional notification.
  notifications_->ShowNotificationDelayed(FileBrowserNotifications::DEVICE_FAIL,
                                          "path",
                                          base::TimeDelta::FromSeconds(0));
  notifications_->ShowNotification(FileBrowserNotifications::DEVICE_FAIL,
                                   "path");
  EXPECT_EQ(2u, notifications_->GetNotificationCountForTest());
  EXPECT_TRUE(FindNotification("DFpath"));

  // When the delayed notification is processed, it's an update, so we still
  // only have two notifications.
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(2u, notifications_->GetNotificationCountForTest());
  EXPECT_TRUE(FindNotification("DFpath"));

  // If we schedule a show for later, then hide before it becomes visible,
  // the notification should not be added.
  notifications_->ShowNotificationDelayed(FileBrowserNotifications::FORMAT_FAIL,
                                          "path",
                                          base::TimeDelta::FromSeconds(0));
  notifications_->HideNotification(FileBrowserNotifications::FORMAT_FAIL,
                                   "path");
  EXPECT_EQ(2u, notifications_->GetNotificationCountForTest());
  EXPECT_TRUE(FindNotification("Dpath"));
  EXPECT_TRUE(FindNotification("DFpath"));
  EXPECT_FALSE(FindNotification("Fpath"));

  // Even after processing messages, no new notification should be added.
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(2u, notifications_->GetNotificationCountForTest());
  EXPECT_TRUE(FindNotification("Dpath"));
  EXPECT_TRUE(FindNotification("DFpath"));
  EXPECT_FALSE(FindNotification("Fpath"));
}

IN_PROC_BROWSER_TEST_F(FileBrowserNotificationsTest, HideDelayedTest) {
  InitNotifications();
  // Showing now, and scheduling a hide for later, results in one notification.
  notifications_->ShowNotification(FileBrowserNotifications::DEVICE, "path");
  notifications_->HideNotificationDelayed(FileBrowserNotifications::DEVICE,
                                          "path",
                                          base::TimeDelta::FromSeconds(0));
  EXPECT_EQ(1u, notifications_->GetNotificationCountForTest());
  EXPECT_TRUE(FindNotification("Dpath"));

  // Running pending messges should remove the notification.
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(0u, notifications_->GetNotificationCountForTest());

  // Immediate show then hide results in no notification.
  notifications_->ShowNotification(FileBrowserNotifications::DEVICE_FAIL,
                                   "path");
  notifications_->HideNotification(FileBrowserNotifications::DEVICE_FAIL,
                                   "path");
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(0u, notifications_->GetNotificationCountForTest());

  // Delayed hide for a notification that doesn't exist does nothing.
  notifications_->HideNotificationDelayed(FileBrowserNotifications::DEVICE_FAIL,
                                          "path",
                                          base::TimeDelta::FromSeconds(0));
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(0u, notifications_->GetNotificationCountForTest());
}

}  // namespace chromeos.
