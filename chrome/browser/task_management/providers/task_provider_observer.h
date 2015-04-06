// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_TASK_PROVIDER_OBSERVER_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_TASK_PROVIDER_OBSERVER_H_

#include "chrome/browser/task_management/providers/task.h"

namespace task_management {

// Defines an interface for observing tasks addition / removal.
class TaskProviderObserver {
 public:
  TaskProviderObserver() {}
  virtual ~TaskProviderObserver() {}

  // This notifies of the event that a new |task| has been created.
  virtual void TaskAdded(Task* task) = 0;

  // This notifies of the event that a |task| is about to be removed. The task
  // is still valid during this call, after that it may never be used again by
  // the observer and references to it must not be kept.
  virtual void TaskRemoved(Task* task) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskProviderObserver);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_TASK_PROVIDER_OBSERVER_H_
