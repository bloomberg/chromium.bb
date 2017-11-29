// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_FALLBACK_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_FALLBACK_TASK_PROVIDER_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/task_manager/providers/task_provider.h"
#include "chrome/browser/task_manager/providers/task_provider_observer.h"

namespace task_manager {

// This FallbackTaskProvider is created to manage 2 subproviders. Tasks from the
// primary subprovider are always shown in the Task Manager.  Tasks from the
// secondary subprovider are only shown when no primary task exists for that
// process. It is currently behind the command line flag
// --task-manager-show-extra-renderers.
class FallbackTaskProvider : public TaskProvider {
 public:
  FallbackTaskProvider(std::unique_ptr<TaskProvider> primary_subprovider,
                       std::unique_ptr<TaskProvider> secondary_subprovider);
  ~FallbackTaskProvider() override;

  // task_manager::TaskProvider:
  Task* GetTaskOfUrlRequest(int child_id, int route_id) override;

 private:
  friend class FallbackTaskProviderTest;
  class SubproviderSource;

  // task_manager::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  // This is used to show a task after |OnTaskAddedBySource| has decided that it
  // is appropriate to show that task.
  void ShowTask(Task* task);

  // Called to add a task to the |pending_shown_tasks_| which delays showing the
  // task for a duration controlled by kTimeDelayForPendingTask.
  void ShowTaskLater(Task* task);

  // This is called after the delay to show the task that has been delayed.
  void ShowPendingTask(Task* task);

  // This is used to hide a task after |OnTaskAddedBySource| has decided that it
  // is appropriate to hide that task.
  void HideTask(Task* task);

  void OnTaskUnresponsive(Task* task);

  void OnTaskAddedBySource(Task* task, SubproviderSource* source);
  void OnTaskRemovedBySource(Task* task, SubproviderSource* source);

  SubproviderSource* primary_source() { return sources_[0].get(); }
  SubproviderSource* secondary_source() { return sources_[1].get(); }

  // Stores the pointers we use to get access to the sources. By convention 0 is
  // the primary source and 1 is the secondary source. This means that the tasks
  // from the primary source will shadow the tasks from the secondary source.
  std::unique_ptr<SubproviderSource> sources_[2];

  // This is the set of tasks that this provider is currently passing up to
  // whatever is observing it.
  std::vector<Task*> shown_tasks_;

  // This maps a Task to a WeakPtrFactory so when a task is removed we can
  // cancel showing a task that has been removed before it has been shown.
  std::map<Task*, base::WeakPtrFactory<FallbackTaskProvider>>
      pending_shown_tasks_;

  DISALLOW_COPY_AND_ASSIGN(FallbackTaskProvider);
};

class FallbackTaskProvider::SubproviderSource : public TaskProviderObserver {
 public:
  SubproviderSource(FallbackTaskProvider* fallback_task_provider,
                    std::unique_ptr<TaskProvider> subprovider);
  ~SubproviderSource() override;

  TaskProvider* subprovider() { return subprovider_.get(); }
  std::vector<Task*>* tasks() { return &tasks_; }

 private:
  friend class FallbackTaskProviderTest;
  void TaskAdded(Task* task) override;
  void TaskRemoved(Task* task) override;
  void TaskUnresponsive(Task* task) override;

  // The outer task provider on whose behalf we observe the |subprovider_|. This
  // is a pointer back to the class that owns us.
  FallbackTaskProvider* fallback_task_provider_;

  // The task provider that we are observing.
  std::unique_ptr<TaskProvider> subprovider_;

  // The vector of tasks that have been created by |subprovider_|.
  std::vector<Task*> tasks_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_FALLBACK_TASK_PROVIDER_H_
