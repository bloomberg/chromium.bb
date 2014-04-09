// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_storage_monitor.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

namespace extensions {

namespace {

const int kInitialUsageThreshold = 500;

const char kWriteDataApp[] = "storage_monitor/write_data";

class NotificationObserver : public message_center::MessageCenterObserver {
 public:
  explicit NotificationObserver(const std::string& target_notification)
      : message_center_(message_center::MessageCenter::Get()),
        target_notification_id_(target_notification),
        waiting_(false) {
    message_center_->AddObserver(this);
  }

  virtual ~NotificationObserver() {
    message_center_->RemoveObserver(this);
  }

  bool HasReceivedNotification() const {
    return received_notifications_.find(target_notification_id_) !=
      received_notifications_.end();
  }

  // Runs the message loop and returns true if a notification is received.
  // Immediately returns true if a notification has already been received.
  bool WaitForNotification() {
    if (HasReceivedNotification())
      return true;

    waiting_ = true;
    content::RunMessageLoop();
    waiting_ = false;
    return HasReceivedNotification();
  }

 private:
  // MessageCenterObserver implementation:
  virtual void OnNotificationAdded(
      const std::string& notification_id) OVERRIDE {
    received_notifications_.insert(notification_id);

    if (waiting_ && HasReceivedNotification())
      base::MessageLoopForUI::current()->Quit();
  }

  message_center::MessageCenter* message_center_;
  std::set<std::string> received_notifications_;
  std::string target_notification_id_;
  bool waiting_;
};

}  // namespace

class ExtensionStorageMonitorTest : public ExtensionBrowserTest {
 protected:
  void InitStorageMonitor() {
    ExtensionStorageMonitor* monitor = ExtensionStorageMonitor::Get(profile());
    ASSERT_TRUE(monitor);

    // Override thresholds so that we don't have to write a huge amount of data
    // to trigger notifications in these tests.
    monitor->enable_for_all_extensions_ = true;
    monitor->initial_extension_threshold_ = kInitialUsageThreshold;

    // To ensure storage events are dispatched from QuotaManager immediately.
    monitor->observer_rate_ = 0;
  }

  const Extension* InitWriteDataApp() {
    InitStorageMonitor();

    base::FilePath path = test_data_dir_.AppendASCII(kWriteDataApp);
    const Extension* extension = InstallExtension(path, 1);
    EXPECT_TRUE(extension);
    return extension;
  }

  std::string GetNotificationId(const std::string& extension_id) {
    return ExtensionStorageMonitor::Get(profile())->
        GetNotificationId(extension_id);
  }

  void WriteBytesExpectingNotification(const Extension* extension,
                                       int num_bytes) {
    WriteBytes(extension, num_bytes, true);
  }

  void WriteBytesNotExpectingNotification(const Extension* extension,
                                         int num_bytes) {
    WriteBytes(extension, num_bytes, false);
  }

private:
  // Write a number of bytes to persistent storage.
  void WriteBytes(const Extension* extension,
                  int num_bytes,
                  bool expected_notification) {
    ExtensionTestMessageListener launched_listener("launched", true);
    ExtensionTestMessageListener write_complete_listener(
        "write_complete", false);
    NotificationObserver notification_observer(
        GetNotificationId(extension->id()));

    OpenApplication(AppLaunchParams(
        profile(), extension, LAUNCH_CONTAINER_NONE, NEW_WINDOW));
    ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

    // Instruct the app to write |num_bytes| of data.
    launched_listener.Reply(base::IntToString(num_bytes));
    ASSERT_TRUE(write_complete_listener.WaitUntilSatisfied());

    if (expected_notification) {
      EXPECT_TRUE(notification_observer.WaitForNotification());
    } else {
      base::RunLoop().RunUntilIdle();
      EXPECT_FALSE(notification_observer.HasReceivedNotification());
    }
  }
};

// Control - No notifications should be shown if usage remains under the
// threshold.
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest, UnderThreshold) {
  const Extension* extension = InitWriteDataApp();
  ASSERT_TRUE(extension);
  WriteBytesNotExpectingNotification(extension, 1);
}

// Ensure a notification is shown when usage reaches the first threshold.
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest, ExceedInitialThreshold) {
  const Extension* extension = InitWriteDataApp();
  ASSERT_TRUE(extension);
  WriteBytesExpectingNotification(extension, kInitialUsageThreshold);
}

// Ensure a notification is shown when usage immediately exceeds double the
// first threshold.
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest, DoubleInitialThreshold) {
  const Extension* extension = InitWriteDataApp();
  ASSERT_TRUE(extension);
  WriteBytesExpectingNotification(extension, kInitialUsageThreshold*2);
}

// Ensure that notifications are not fired if the next threshold has not been
// reached.
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest, ThrottleNotifications) {
  const Extension* extension = InitWriteDataApp();
  ASSERT_TRUE(extension);

  // Exceed the first threshold.
  WriteBytesExpectingNotification(extension, kInitialUsageThreshold);

  // Stay within the next threshold.
  WriteBytesNotExpectingNotification(extension, 1);
}

// Verify that notifications are disabled when the user clicks the action button
// in the notification.
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest, UserDisabledNotifications) {
  const Extension* extension = InitWriteDataApp();
  ASSERT_TRUE(extension);
  WriteBytesExpectingNotification(extension, kInitialUsageThreshold);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  ASSERT_TRUE(prefs);
  EXPECT_TRUE(prefs->IsStorageNotificationEnabled(extension->id()));

  // Fake clicking the notification button.
  message_center::MessageCenter::Get()->ClickOnNotificationButton(
      GetNotificationId(extension->id()),
      ExtensionStorageMonitor::BUTTON_DISABLE_NOTIFICATION);

  EXPECT_FALSE(prefs->IsStorageNotificationEnabled(extension->id()));

  // Expect to receive no further notifications when usage continues to
  // increase.
  int64 next_threshold = prefs->GetNextStorageThreshold(extension->id());
  int64 next_data_size = next_threshold - kInitialUsageThreshold;
  ASSERT_GE(next_data_size, 0);

  WriteBytesNotExpectingNotification(extension, next_data_size);
}

}  // namespace extensions
