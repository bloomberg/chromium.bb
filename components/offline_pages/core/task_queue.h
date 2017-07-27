// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_TASK_QUEUE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_TASK_QUEUE_H_

#include <memory>
#include <queue>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/task.h"

namespace offline_pages {

// Class for coordinating |Task|s in relation to access to a specific resource.
// As a task, we understand a set of asynchronous operations (possibly switching
// threads) that access a set of sensitive resource(s). Because the resource
// state is modified and individual steps of a task are asynchronous, allowing
// certain tasks to run in parallel may lead to incorrect results. This class
// allows for ordering of tasks in a FIFO manner, to ensure two tasks modifying
// a resources are not run at the same time.
//
// Consumers of this class should create an instance of TaskQueue and implement
// tasks that need to be run sequentially. New task will only be started when
// the previous one calls |Task::TaskComplete|.
class TaskQueue {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {};

    // Invoked once when TaskQueue reached 0 tasks.
    virtual void OnTaskQueueIsIdle() = 0;
  };

  explicit TaskQueue(Delegate* delegate);
  ~TaskQueue();

  // Adds a task to the queue. Queue takes ownership of the task.
  void AddTask(std::unique_ptr<Task> task);
  // Whether the task queue has any pending (not-running) tasks.
  bool HasPendingTasks() const;
  // Whether there is a task currently running.
  bool HasRunningTask() const;

 private:
  // Checks whether there are any tasks to run, as well as whether no task is
  // currently running. When both are met, it will start the next task in the
  // queue.
  void StartTaskIfAvailable();

  // Callback for informing the queue that a task was completed.
  void TaskCompleted(Task* task);

  void InformTaskQueueIsIdle();

  // Owns and outlives this TaskQueue.
  Delegate* delegate_;

  // Currently running tasks.
  std::unique_ptr<Task> current_task_;

  // A FIFO queue of tasks that will be run using this task queue.
  std::queue<std::unique_ptr<Task>> tasks_;

  base::WeakPtrFactory<TaskQueue> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueue);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_TASK_QUEUE_H_
