// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/task_management_browsertest_util.h"

#include "base/stl_util.h"

namespace task_management {

MockWebContentsTaskManager::MockWebContentsTaskManager()
    : tasks_(),
      provider_() {
}

MockWebContentsTaskManager::~MockWebContentsTaskManager() {
}

void MockWebContentsTaskManager::TaskAdded(Task* task) {
  DCHECK(task);
  DCHECK(!ContainsValue(tasks_, task));
  tasks_.push_back(task);
}

void MockWebContentsTaskManager::TaskRemoved(Task* task) {
  DCHECK(task);
  DCHECK(ContainsValue(tasks_, task));
  tasks_.erase(std::find(tasks_.begin(), tasks_.end(), task));
}

void MockWebContentsTaskManager::StartObserving() {
  provider_.SetObserver(this);
}

void MockWebContentsTaskManager::StopObserving() {
  provider_.ClearObserver();
}

}  // namespace task_management
