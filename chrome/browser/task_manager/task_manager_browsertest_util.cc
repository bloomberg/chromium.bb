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
#include "chrome/test/base/ui_test_utils.h"
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
  ui_test_utils::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TASK_MANAGER_WINDOW_READY,
      Source<TaskManagerModel>(TaskManager::GetInstance()->model()));
  browser->window()->ShowTaskManager();
  observer.Wait();
#else
  browser->window()->ShowTaskManager();
#endif  // defined(WEBUI_TASK_MANAGER)
}

