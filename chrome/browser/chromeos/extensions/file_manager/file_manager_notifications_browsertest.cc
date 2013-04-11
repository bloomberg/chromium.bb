// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_manager_notifications.h"

#include <gtest/gtest.h>
#include <string>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

class FileManagerNotificationsTest : public InProcessBrowserTest {
 public:
  FileManagerNotificationsTest() {}

  virtual void CleanUpOnMainThread() OVERRIDE {
    notifications_.reset();
  }

 protected:
  // This must be initialized late in test startup.
  void InitNotifications() {
    Profile* profile = browser()->profile();
    notifications_.reset(new FileManagerNotifications(profile));
  }

  bool FindNotification(const std::string& id) {
    return notifications_->HasNotificationForTest(id);
  }

  scoped_ptr<FileManagerNotifications> notifications_;
};

IN_PROC_BROWSER_TEST_F(FileManagerNotificationsTest, TestBasic) {
  InitNotifications();
  // Showing a notification adds a new notification.
  notifications_->ShowNotification(FileManagerNotifications::DEVICE, "path");
  EXPECT_EQ(1u, notifications_->GetNotificationCountForTest());
  EXPECT_TRUE(FindNotification("Device_path"));

  // Updating the same notification maintains the same count.
  notifications_->ShowNotification(FileManagerNotifications::DEVICE, "path");
  EXPECT_EQ(1u, notifications_->GetNotificationCountForTest());
  EXPECT_TRUE(FindNotification("Device_path"));

  // A new notification increases the count.
  notifications_->ShowNotification(FileManagerNotifications::DEVICE_FAIL,
                                   "path");
  EXPECT_EQ(2u, notifications_->GetNotificationCountForTest());
  EXPECT_TRUE(FindNotification("DeviceFail_path"));
  EXPECT_TRUE(FindNotification("Device_path"));

  // Hiding a notification removes it from our data.
  notifications_->HideNotification(FileManagerNotifications::DEVICE_FAIL,
                                   "path");
  EXPECT_EQ(1u, notifications_->GetNotificationCountForTest());
  EXPECT_FALSE(FindNotification("DeviceFail_path"));
  EXPECT_TRUE(FindNotification("Device_path"));
};

// Note: Delayed tests use a delay time of 0 so that tasks wille execute
// when RunAllPendingInMessageLoop() is called.
IN_PROC_BROWSER_TEST_F(FileManagerNotificationsTest, ShowDelayedTest) {
  InitNotifications();
  // Adding a delayed notification does not create a notification.
  notifications_->ShowNotificationDelayed(FileManagerNotifications::DEVICE,
                                          "path",
                                          base::TimeDelta::FromSeconds(0));
  EXPECT_EQ(0u, notifications_->GetNotificationCountForTest());
  EXPECT_FALSE(FindNotification("Device_path"));

  // Running the message loop should create the notification.
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(1u, notifications_->GetNotificationCountForTest());
  EXPECT_TRUE(FindNotification("Device_path"));

  // Showing a notification both immediately and delayed results in one
  // additional notification.
  notifications_->ShowNotificationDelayed(FileManagerNotifications::DEVICE_FAIL,
                                          "path",
                                          base::TimeDelta::FromSeconds(0));
  notifications_->ShowNotification(FileManagerNotifications::DEVICE_FAIL,
                                   "path");
  EXPECT_EQ(2u, notifications_->GetNotificationCountForTest());
  EXPECT_TRUE(FindNotification("DeviceFail_path"));

  // When the delayed notification is processed, it's an update, so we still
  // only have two notifications.
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(2u, notifications_->GetNotificationCountForTest());
  EXPECT_TRUE(FindNotification("DeviceFail_path"));

  // If we schedule a show for later, then hide before it becomes visible,
  // the notification should not be added.
  notifications_->ShowNotificationDelayed(FileManagerNotifications::FORMAT_FAIL,
                                          "path",
                                          base::TimeDelta::FromSeconds(0));
  notifications_->HideNotification(FileManagerNotifications::FORMAT_FAIL,
                                   "path");
  EXPECT_EQ(2u, notifications_->GetNotificationCountForTest());
  EXPECT_TRUE(FindNotification("Device_path"));
  EXPECT_TRUE(FindNotification("DeviceFail_path"));
  EXPECT_FALSE(FindNotification("Format_path"));

  // Even after processing messages, no new notification should be added.
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(2u, notifications_->GetNotificationCountForTest());
  EXPECT_TRUE(FindNotification("Device_path"));
  EXPECT_TRUE(FindNotification("DeviceFail_path"));
  EXPECT_FALSE(FindNotification("Format_path"));
}

IN_PROC_BROWSER_TEST_F(FileManagerNotificationsTest, HideDelayedTest) {
  InitNotifications();
  // Showing now, and scheduling a hide for later, results in one notification.
  notifications_->ShowNotification(FileManagerNotifications::DEVICE, "path");
  notifications_->HideNotificationDelayed(FileManagerNotifications::DEVICE,
                                          "path",
                                          base::TimeDelta::FromSeconds(0));
  EXPECT_EQ(1u, notifications_->GetNotificationCountForTest());
  EXPECT_TRUE(FindNotification("Device_path"));

  // Running pending messges should remove the notification.
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(0u, notifications_->GetNotificationCountForTest());

  // Immediate show then hide results in no notification.
  notifications_->ShowNotification(FileManagerNotifications::DEVICE_FAIL,
                                   "path");
  notifications_->HideNotification(FileManagerNotifications::DEVICE_FAIL,
                                   "path");
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(0u, notifications_->GetNotificationCountForTest());

  // Delayed hide for a notification that doesn't exist does nothing.
  notifications_->HideNotificationDelayed(FileManagerNotifications::DEVICE_FAIL,
                                          "path",
                                          base::TimeDelta::FromSeconds(0));
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(0u, notifications_->GetNotificationCountForTest());
}

// Tests that showing two notifications with the same notification ids and
// different messages results in showing the second notification's message.
// This situation can be encountered while showing notifications for
// MountCompletedEvent.
IN_PROC_BROWSER_TEST_F(FileManagerNotificationsTest, IdenticalNotificationIds) {
  InitNotifications();
  notifications_->RegisterDevice("path");

  notifications_->ManageNotificationsOnMountCompleted("path", "", false, false,
      false);
  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(
    l10n_util::GetStringUTF16(IDS_DEVICE_UNKNOWN_DEFAULT_MESSAGE),
    notifications_->GetNotificationMessageForTest("DeviceFail_path"));

  notifications_->ManageNotificationsOnMountCompleted("path", "", false, false,
      false);
  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(
    l10n_util::GetStringUTF16(IDS_MULTIPART_DEVICE_UNSUPPORTED_DEFAULT_MESSAGE),
    notifications_->GetNotificationMessageForTest("DeviceFail_path"));

  notifications_->UnregisterDevice("path");
}

}  // namespace chromeos.
