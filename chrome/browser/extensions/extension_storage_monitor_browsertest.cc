// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <set>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_storage_monitor.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/constants.h"
#include "extensions/test/extension_test_message_listener.h"
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

  ~NotificationObserver() override { message_center_->RemoveObserver(this); }

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
  void OnNotificationAdded(const std::string& notification_id) override {
    received_notifications_.insert(notification_id);

    if (waiting_ && HasReceivedNotification())
      base::MessageLoopForUI::current()->QuitWhenIdle();
  }

  message_center::MessageCenter* message_center_;
  std::set<std::string> received_notifications_;
  std::string target_notification_id_;
  bool waiting_;
};

}  // namespace

class ExtensionStorageMonitorTest : public ExtensionBrowserTest {
 public:
  ExtensionStorageMonitorTest() : storage_monitor_(NULL) {}

 protected:
  // ExtensionBrowserTest overrides:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    InitStorageMonitor();
  }

  ExtensionStorageMonitor* monitor() {
    CHECK(storage_monitor_);
    return storage_monitor_;
  }

  int64_t GetInitialExtensionThreshold() {
    CHECK(storage_monitor_);
    return storage_monitor_->initial_extension_threshold_;
  }

  void DisableForInstalledExtensions() {
    CHECK(storage_monitor_);
    storage_monitor_->enable_for_all_extensions_ = false;
  }

  const Extension* InitWriteDataApp() {
    base::FilePath path = test_data_dir_.AppendASCII(kWriteDataApp);
    const Extension* extension = InstallExtension(path, 1);
    EXPECT_TRUE(extension);
    return extension;
  }

  std::string GetNotificationId(const std::string& extension_id) {
    return monitor()->GetNotificationId(extension_id);
  }

  bool IsStorageNotificationEnabled(const std::string& extension_id) {
    return monitor()->IsStorageNotificationEnabled(extension_id);
  }

  int64_t GetNextStorageThreshold(const std::string& extension_id) {
    return monitor()->GetNextStorageThreshold(extension_id);
  }

  void WriteBytesExpectingNotification(const Extension* extension,
                                       int num_bytes) {
    int64_t previous_threshold = GetNextStorageThreshold(extension->id());
    WriteBytes(extension, num_bytes, true);
    EXPECT_GT(GetNextStorageThreshold(extension->id()), previous_threshold);
  }

  void WriteBytesNotExpectingNotification(const Extension* extension,
                                         int num_bytes) {
    WriteBytes(extension, num_bytes, false);
  }

 private:
  void InitStorageMonitor() {
    storage_monitor_ = ExtensionStorageMonitor::Get(profile());
    ASSERT_TRUE(storage_monitor_);

    // Override thresholds so that we don't have to write a huge amount of data
    // to trigger notifications in these tests.
    storage_monitor_->enable_for_all_extensions_ = true;
    storage_monitor_->initial_extension_threshold_ = kInitialUsageThreshold;

    // To ensure storage events are dispatched from QuotaManager immediately.
    storage_monitor_->observer_rate_ = base::TimeDelta();
  }

  // Write a number of bytes to persistent storage.
  void WriteBytes(const Extension* extension,
                  int num_bytes,
                  bool expected_notification) {
    ExtensionTestMessageListener launched_listener("launched", true);
    ExtensionTestMessageListener write_complete_listener(
        "write_complete", false);
    NotificationObserver notification_observer(
        GetNotificationId(extension->id()));

    OpenApplication(AppLaunchParams(profile(), extension, LAUNCH_CONTAINER_NONE,
                                    WindowOpenDisposition::NEW_WINDOW,
                                    extensions::SOURCE_TEST));
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

  ExtensionStorageMonitor* storage_monitor_;
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
  WriteBytesExpectingNotification(extension, GetInitialExtensionThreshold());
}

// Ensure a notification is shown when usage immediately exceeds double the
// first threshold.
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest, DoubleInitialThreshold) {
  const Extension* extension = InitWriteDataApp();
  ASSERT_TRUE(extension);
  WriteBytesExpectingNotification(extension,
                                  GetInitialExtensionThreshold() * 2);
}

// Ensure that notifications are not fired if the next threshold has not been
// reached.
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest, ThrottleNotifications) {
  const Extension* extension = InitWriteDataApp();
  ASSERT_TRUE(extension);

  // Exceed the first threshold.
  WriteBytesExpectingNotification(extension, GetInitialExtensionThreshold());

  // Stay within the next threshold.
  WriteBytesNotExpectingNotification(extension, 1);
}

// Verify that notifications are disabled when the user clicks the action button
// in the notification.
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest, UserDisabledNotifications) {
  const Extension* extension = InitWriteDataApp();
  ASSERT_TRUE(extension);
  WriteBytesExpectingNotification(extension, GetInitialExtensionThreshold());

  EXPECT_TRUE(IsStorageNotificationEnabled(extension->id()));

  // Fake clicking the notification button to disable notifications.
  message_center::MessageCenter::Get()->ClickOnNotificationButton(
      GetNotificationId(extension->id()),
      ExtensionStorageMonitor::BUTTON_DISABLE_NOTIFICATION);

  EXPECT_FALSE(IsStorageNotificationEnabled(extension->id()));

  // Expect to receive no further notifications when usage continues to
  // increase.
  int64_t next_threshold = GetNextStorageThreshold(extension->id());
  int64_t next_data_size = next_threshold - GetInitialExtensionThreshold();
  ASSERT_GT(next_data_size, 0);

  WriteBytesNotExpectingNotification(extension, next_data_size);
}

// Ensure that monitoring is disabled for installed extensions if
// |enable_for_all_extensions_| is false. This test can be removed if monitoring
// is eventually enabled for all extensions.
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest,
                       DisableForInstalledExtensions) {
  DisableForInstalledExtensions();

  const Extension* extension = InitWriteDataApp();
  ASSERT_TRUE(extension);
  WriteBytesNotExpectingNotification(extension, GetInitialExtensionThreshold());
}

// Verify that notifications are disabled when the user clicks the action button
// in the notification.
// Flaky: https://crbug.com/617801
IN_PROC_BROWSER_TEST_F(ExtensionStorageMonitorTest,
                       DISABLED_UninstallExtension) {
  const Extension* extension = InitWriteDataApp();
  ASSERT_TRUE(extension);
  WriteBytesExpectingNotification(extension, GetInitialExtensionThreshold());

  // Fake clicking the notification button to uninstall and accepting the
  // uninstall.
  ScopedTestDialogAutoConfirm scoped_autoconfirm(
      ScopedTestDialogAutoConfirm::ACCEPT);
  TestExtensionRegistryObserver observer(ExtensionRegistry::Get(profile()),
                                         extension->id());
  message_center::MessageCenter::Get()->ClickOnNotificationButton(
      GetNotificationId(extension->id()),
      ExtensionStorageMonitor::BUTTON_UNINSTALL);
  observer.WaitForExtensionUninstalled();
}

}  // namespace extensions
