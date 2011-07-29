// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ResourceChangeObserver : public TaskManagerModelObserver {
 public:
  ResourceChangeObserver(const TaskManagerModel* model,
                         int target_resource_count)
      : model_(model),
        target_resource_count_(target_resource_count) {
  }

  virtual void OnModelChanged() {
    OnResourceChange();
  }

  virtual void OnItemsChanged(int start, int length) {
    OnResourceChange();
  }

  virtual void OnItemsAdded(int start, int length) {
    OnResourceChange();
  }

  virtual void OnItemsRemoved(int start, int length) {
    OnResourceChange();
  }

 private:
  void OnResourceChange() {
    if (model_->ResourceCount() == target_resource_count_)
      MessageLoopForUI::current()->Quit();
  }

  const TaskManagerModel* model_;
  const int target_resource_count_;
};

}  // namespace

class TaskManagerNotificationBrowserTest : public ExtensionBrowserTest {
 public:
  TaskManagerModel* model() const {
    return TaskManager::GetInstance()->model();
  }

  void WaitForResourceChange(int target_count) {
    if (model()->ResourceCount() == target_count)
      return;
    ResourceChangeObserver observer(model(), target_count);
    model()->AddObserver(&observer);
    ui_test_utils::RunMessageLoop();
    model()->RemoveObserver(&observer);
  }
};

IN_PROC_BROWSER_TEST_F(TaskManagerNotificationBrowserTest,
                       NoticeNotificationChanges) {
  EXPECT_EQ(0, model()->ResourceCount());

  // Show the task manager.
  browser()->window()->ShowTaskManager();
  // Expect to see the browser and the New Tab Page renderer.
  WaitForResourceChange(2);

  // Show a notification.
  NotificationUIManager* notifications =
      g_browser_process->notification_ui_manager();

  string16 content = DesktopNotificationService::CreateDataUrl(
      GURL(), ASCIIToUTF16("Hello World!"), string16(),
      WebKit::WebTextDirectionDefault);

  scoped_refptr<NotificationDelegate> del1(new MockNotificationDelegate("n1"));
  Notification n1(
      GURL(), GURL(content), ASCIIToUTF16("Test 1"), string16(), del1.get());
  scoped_refptr<NotificationDelegate> del2(new MockNotificationDelegate("n2"));
  Notification n2(
      GURL(), GURL(content), ASCIIToUTF16("Test 2"), string16(), del2.get());

  notifications->Add(n1, browser()->profile());
  WaitForResourceChange(3);
  notifications->Add(n2, browser()->profile());
  WaitForResourceChange(4);
  notifications->CancelById(n1.notification_id());
  WaitForResourceChange(3);
  notifications->CancelById(n2.notification_id());
  WaitForResourceChange(2);
}
