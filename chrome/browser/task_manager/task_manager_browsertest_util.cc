// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"

namespace {

int GetWebResourceCount(const TaskManagerModel* model) {
  int count = 0;
  for (int i = 0; i < model->ResourceCount(); i++) {
    task_manager::Resource::Type type = model->GetResourceType(i);
    // Skip system infrastructure resources.
    if (type == task_manager::Resource::BROWSER ||
        type == task_manager::Resource::NACL ||
        type == task_manager::Resource::GPU ||
        type == task_manager::Resource::UTILITY ||
        type == task_manager::Resource::ZYGOTE ||
        type == task_manager::Resource::SANDBOX_HELPER) {
      continue;
    }

    count++;
  }
  return count;
}

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
    if (GetWebResourceCount(model_) == target_resource_count_)
      base::MessageLoopForUI::current()->Quit();
  }

  const TaskManagerModel* model_;
  const int target_resource_count_;
};

}  // namespace

// static
void TaskManagerBrowserTestUtil::WaitForWebResourceChange(int target_count) {
  TaskManagerModel* model = TaskManager::GetInstance()->model();

  ResourceChangeObserver observer(model, target_count);
  model->AddObserver(&observer);

  // Checks that the condition has not been satisfied yet.
  // This check has to be placed after the installation of the observer,
  // because resources may change before that.
  if (GetWebResourceCount(model) == target_count) {
    model->RemoveObserver(&observer);
    return;
  }

  content::RunMessageLoop();
  model->RemoveObserver(&observer);
}
