// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center_switches.h"
#include "ui/message_center/message_center_util.h"

using task_manager::browsertest_util::WaitForTaskManagerRows;

class TaskManagerNotificationBrowserTest : public ExtensionBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
  }

  // Returns the text we expect to see in the TaskManager for a given
  // notification.
  base::string16 GetTitle(const char* ascii_name) {
    return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_NOTIFICATION_PREFIX,
                                      base::ASCIIToUTF16(ascii_name));
  }
};

// TODO(linux_aura) http://crbug.com/163931
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
#define MAYBE_NoticeNotificationChanges DISABLED_NoticeNotificationChanges
#else
#define MAYBE_NoticeNotificationChanges NoticeNotificationChanges
#endif
IN_PROC_BROWSER_TEST_F(TaskManagerNotificationBrowserTest,
                       MAYBE_NoticeNotificationChanges) {
  // These tests do not apply with Message Center-only platforms (e.g. Ash)
  // where notifications do not instantiate a new renderer.
  if (message_center::IsRichNotificationEnabled())
    return;

  // Show a notification.
  NotificationUIManager* notifications =
      g_browser_process->notification_ui_manager();

  base::string16 content = DesktopNotificationService::CreateDataUrl(
      GURL(), base::ASCIIToUTF16("Hello World!"), base::string16(),
      blink::WebTextDirectionDefault);

  // Show an initial notification before popping up the task manager.
  scoped_refptr<NotificationDelegate> del0(new MockNotificationDelegate("n0"));
  Notification n0(
      GURL(), GURL(content), base::ASCIIToUTF16("Test 0"), base::string16(),
      del0.get());
  notifications->Add(n0, browser()->profile());

  // Show the task manager.
  chrome::ShowTaskManager(browser());

  // This notification should show up in the task manager.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, GetTitle("Test 0")));

  scoped_refptr<NotificationDelegate> del1(new MockNotificationDelegate("n1"));
  Notification n1(
      GURL(), GURL(content), base::ASCIIToUTF16("Test 1"), base::string16(),
      del1.get());
  scoped_refptr<NotificationDelegate> del2(new MockNotificationDelegate("n2"));
  Notification n2(
      GURL(), GURL(content), base::ASCIIToUTF16("Test 2"), base::string16(),
      del2.get());

  // Show two more notifications with the task manager running, then cancel
  // all three notifications.
  notifications->Add(n1, browser()->profile());
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, GetTitle("Test 0")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, GetTitle("Test 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, GetTitle("Test 2")));

  notifications->Add(n2, browser()->profile());
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, GetTitle("Test 0")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, GetTitle("Test 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, GetTitle("Test 2")));

  notifications->CancelById(n1.notification_id());
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, GetTitle("Test 0")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, GetTitle("Test 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, GetTitle("Test 2")));

  notifications->CancelById(n0.notification_id());
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, GetTitle("Test 0")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, GetTitle("Test 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, GetTitle("Test 2")));

  notifications->CancelById(n2.notification_id());
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, GetTitle("Test 0")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, GetTitle("Test 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, GetTitle("Test 2")));
}
