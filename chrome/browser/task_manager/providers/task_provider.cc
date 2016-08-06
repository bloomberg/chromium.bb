// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/task_provider.h"

namespace task_manager {

TaskProvider::TaskProvider()
    : observer_(nullptr) {
}

TaskProvider::~TaskProvider() {
}

void TaskProvider::SetObserver(TaskProviderObserver* observer) {
  DCHECK(observer);
  DCHECK(!observer_);
  observer_ = observer;
  StartUpdating();
}

void TaskProvider::ClearObserver() {
  DCHECK(observer_);
  observer_ = nullptr;
  StopUpdating();
}

void TaskProvider::NotifyObserverTaskAdded(Task* task) const {
  DCHECK(observer_);
  observer_->TaskAdded(task);
}

void TaskProvider::NotifyObserverTaskRemoved(Task* task) const {
  DCHECK(observer_);
  observer_->TaskRemoved(task);
}

void TaskProvider::NotifyObserverTaskUnresponsive(Task* task) const {
  DCHECK(observer_);
  observer_->TaskUnresponsive(task);
}

}  // namespace task_manager
