// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_INSTALLABLE_TASK_QUEUE_H_
#define CHROME_BROWSER_INSTALLABLE_INSTALLABLE_TASK_QUEUE_H_

#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/installable/installable_params.h"

#include "base/callback.h"

using InstallableTask = std::pair<InstallableParams, InstallableCallback>;

// InstallableTaskQueue keeps track of pending tasks.
class InstallableTaskQueue {
 public:
  InstallableTaskQueue();
  ~InstallableTaskQueue();

  // Adds task to the end of the active list of tasks to be processed.
  void Insert(InstallableTask task);

  // Moves the current task from the main to the paused list.
  void PauseCurrent();

  // Reports whether there are any tasks in the paused list.
  bool HasPaused() const;

  // Moves all paused tasks to the main list.
  void UnpauseAll();

  // Returns the currently active task.
  InstallableTask& Current();

  // Advances to the next task.
  void Next();

  // Clears all tasks from the main and paused list.
  void Reset();

  // Reports whether the main list is empty.
  bool IsEmpty() const;

 private:
  // The list of <params, callback> pairs that have come from a call to
  // InstallableManager::GetData.
  std::vector<InstallableTask> tasks_;

  // Tasks which are waiting indefinitely for a service worker to be detected.
  std::vector<InstallableTask> paused_tasks_;
};

#endif  // CHROME_BROWSER_INSTALLABLE_INSTALLABLE_TASK_QUEUE_H_
