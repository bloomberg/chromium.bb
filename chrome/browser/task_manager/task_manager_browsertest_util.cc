// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/ui_test_utils.h"
#include "content/common/notification_source.h"

namespace {

class ResourceChangeObserver : public TaskManagerModelObserver {
 public:
  ResourceChangeObserver(const TaskManagerModel* model,
                         int target_resource_count)
      : model_(model),
        target_resource_count_(target_resource_count) {
  }

  virtual void OnModelChanged() OVERRIDE {
    OnResourceChange();
  }

  virtual void OnItemsChanged(int start, int length) OVERRIDE {
    OnResourceChange();
  }

  virtual void OnItemsAdded(int start, int length) OVERRIDE {
    OnResourceChange();
  }

  virtual void OnItemsRemoved(int start, int length) OVERRIDE {
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

// Helper class used to wait for a TaskManager to be ready.
class TaskManagerReadyListener : public NotificationObserver {
 public:
  explicit TaskManagerReadyListener(TaskManagerModel* model) {
    registrar_.Add(this, chrome::NOTIFICATION_TASK_MANAGER_WINDOW_READY,
                   Source<TaskManagerModel>(model));
  }

  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    // Quit once the BackgroundContents has been loaded.
    if (type == chrome::NOTIFICATION_TASK_MANAGER_WINDOW_READY)
      MessageLoopForUI::current()->Quit();
  }
 private:
  NotificationRegistrar registrar_;
};

// Helper class used to wait for a BackgroundContents to finish loading.
class BackgroundContentsListener : public NotificationObserver {
 public:
  explicit BackgroundContentsListener(Profile* profile) {
    registrar_.Add(this, chrome::NOTIFICATION_BACKGROUND_CONTENTS_NAVIGATED,
                   Source<Profile>(profile));
  }

  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    // Quit once the BackgroundContents has been loaded.
    if (type == chrome::NOTIFICATION_BACKGROUND_CONTENTS_NAVIGATED)
      MessageLoopForUI::current()->Quit();
  }

  void ShowTaskManagerAndWait() {
    ui_test_utils::RunMessageLoop();
  }

 private:
  NotificationRegistrar registrar_;
};

}  // namespace

// static
void TaskManagerBrowserTestUtil::WaitForResourceChange(int target_count) {
  TaskManagerModel* model = TaskManager::GetInstance()->model();

  if (model->ResourceCount() == target_count)
    return;
  ResourceChangeObserver observer(model, target_count);
  model->AddObserver(&observer);
  ui_test_utils::RunMessageLoop();
  model->RemoveObserver(&observer);
}

// static
void TaskManagerBrowserTestUtil::ShowTaskManagerAndWaitForReady(
    Browser* browser) {
#if defined(WEBUI_TASK_MANAGER)
  TaskManagerReadyListener listener(TaskManager::GetInstance()->model());
  browser->window()->ShowTaskManager();
  ui_test_utils::RunMessageLoop();
#else
  browser->window()->ShowTaskManager();
#endif  // defined(WEBUI_TASK_MANAGER)
}

// static
void TaskManagerBrowserTestUtil::WaitForBackgroundContents(Browser* browser) {
  BackgroundContentsListener listener(browser->profile());
  ui_test_utils::RunMessageLoop();
}

