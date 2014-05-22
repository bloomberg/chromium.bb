// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/notifications/notifications_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/features/feature.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notifier_settings.h"

using extensions::Extension;

namespace utils = extension_function_test_utils;

namespace {

class NotificationsApiTest : public ExtensionApiTest {
 public:
  const extensions::Extension* LoadExtensionAndWait(
      const std::string& test_name) {
    base::FilePath extdir = test_data_dir_.AppendASCII(test_name);
    content::WindowedNotificationObserver page_created(
        chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
        content::NotificationService::AllSources());
    const extensions::Extension* extension = LoadExtension(extdir);
    if (extension) {
      page_created.Wait();
    }
    return extension;
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestBasicUsage) {
  ASSERT_TRUE(RunExtensionTest("notifications/api/basic_usage")) << message_;
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestEvents) {
  ASSERT_TRUE(RunExtensionTest("notifications/api/events")) << message_;
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestCSP) {
  ASSERT_TRUE(RunExtensionTest("notifications/api/csp")) << message_;
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestByUser) {
  const extensions::Extension* extension =
      LoadExtensionAndWait("notifications/api/by_user");
  ASSERT_TRUE(extension) << message_;

  {
    ResultCatcher catcher;
    g_browser_process->message_center()->RemoveNotification(
        extension->id() + "-FOO",
        false);
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }

  {
    ResultCatcher catcher;
    g_browser_process->message_center()->RemoveNotification(
        extension->id() + "-BAR",
        true);
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }

  {
    ResultCatcher catcher;
    g_browser_process->message_center()->RemoveAllNotifications(false);
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
  {
    ResultCatcher catcher;
    g_browser_process->message_center()->RemoveAllNotifications(true);
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestPartialUpdate) {
  ASSERT_TRUE(RunExtensionTest("notifications/api/partial_update")) << message_;
  const extensions::Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  const char kNewTitle[] = "Changed!";
  const char kNewMessage[] = "Too late! The show ended yesterday";
  int kNewPriority = 2;

  const message_center::NotificationList::Notifications& notifications =
      g_browser_process->message_center()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  message_center::Notification* notification = *(notifications.begin());
  LOG(INFO) << "Notification ID: " << notification->id();

  EXPECT_EQ(base::ASCIIToUTF16(kNewTitle), notification->title());
  EXPECT_EQ(base::ASCIIToUTF16(kNewMessage), notification->message());
  EXPECT_EQ(kNewPriority, notification->priority());
  EXPECT_EQ(0u, notification->buttons().size());
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestGetPermissionLevel) {
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());

  // Get permission level for the extension whose notifications are enabled.
  {
    scoped_refptr<extensions::NotificationsGetPermissionLevelFunction>
        notification_function(
            new extensions::NotificationsGetPermissionLevelFunction());

    notification_function->set_extension(empty_extension.get());
    notification_function->set_has_callback(true);

    scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
        notification_function.get(),
        "[]",
        browser(),
        utils::NONE));

    EXPECT_EQ(base::Value::TYPE_STRING, result->GetType());
    std::string permission_level;
    EXPECT_TRUE(result->GetAsString(&permission_level));
    EXPECT_EQ("granted", permission_level);
  }

  // Get permission level for the extension whose notifications are disabled.
  {
    scoped_refptr<extensions::NotificationsGetPermissionLevelFunction>
        notification_function(
            new extensions::NotificationsGetPermissionLevelFunction());

    notification_function->set_extension(empty_extension.get());
    notification_function->set_has_callback(true);

    message_center::NotifierId notifier_id(
        message_center::NotifierId::APPLICATION,
        empty_extension->id());
    message_center::Notifier notifier(notifier_id, base::string16(), true);
    g_browser_process->message_center()->GetNotifierSettingsProvider()->
        SetNotifierEnabled(notifier, false);

    scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
        notification_function.get(),
        "[]",
        browser(),
        utils::NONE));

    EXPECT_EQ(base::Value::TYPE_STRING, result->GetType());
    std::string permission_level;
    EXPECT_TRUE(result->GetAsString(&permission_level));
    EXPECT_EQ("denied", permission_level);
  }
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestOnPermissionLevelChanged) {
  const extensions::Extension* extension =
      LoadExtensionAndWait("notifications/api/permission");
  ASSERT_TRUE(extension) << message_;

  // Test permission level changing from granted to denied.
  {
    ResultCatcher catcher;

    message_center::NotifierId notifier_id(
        message_center::NotifierId::APPLICATION,
        extension->id());
    message_center::Notifier notifier(notifier_id, base::string16(), true);
    g_browser_process->message_center()->GetNotifierSettingsProvider()->
        SetNotifierEnabled(notifier, false);

    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }

  // Test permission level changing from denied to granted.
  {
    ResultCatcher catcher;

    message_center::NotifierId notifier_id(
        message_center::NotifierId::APPLICATION,
        extension->id());
    message_center::Notifier notifier(notifier_id, base::string16(), false);
    g_browser_process->message_center()->GetNotifierSettingsProvider()->
        SetNotifierEnabled(notifier, true);

    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
}
