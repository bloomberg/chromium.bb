// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/synchronous_task_graph_runner.h"

#include "base/threading/simple_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"

namespace cc {

SynchronousTaskGraphRunner::SynchronousTaskGraphRunner() {}

SynchronousTaskGraphRunner::~SynchronousTaskGraphRunner() {
  DCHECK(!work_queue_.HasReadyToRunTasks());
}

NamespaceToken SynchronousTaskGraphRunner::GetNamespaceToken() {
  return work_queue_.GetNamespaceToken();
}

void SynchronousTaskGraphRunner::ScheduleTasks(NamespaceToken token,
                                               TaskGraph* graph) {
  TRACE_EVENT2("cc", "SynchronousTaskGraphRunner::ScheduleTasks", "num_nodes",
               graph->nodes.size(), "num_edges", graph->edges.size());

  DCHECK(token.IsValid());
  DCHECK(!TaskGraphWorkQueue::DependencyMismatch(graph));

  work_queue_.ScheduleTasks(token, graph);
}

void SynchronousTaskGraphRunner::WaitForTasksToFinishRunning(
    NamespaceToken token) {
  TRACE_EVENT0("cc", "SynchronousTaskGraphRunner::WaitForTasksToFinishRunning");

  DCHECK(token.IsValid());
  auto* task_namespace = work_queue_.GetNamespaceForToken(token);

  if (!task_namespace)
    return;

  while (
      !TaskGraphWorkQueue::HasFinishedRunningTasksInNamespace(task_namespace)) {
    RunTask();
  }
}

void SynchronousTaskGraphRunner::CollectCompletedTasks(
    NamespaceToken token,
    Task::Vector* completed_tasks) {
  TRACE_EVENT0("cc", "SynchronousTaskGraphRunner::CollectCompletedTasks");

  DCHECK(token.IsValid());
  work_queue_.CollectCompletedTasks(token, completed_tasks);
}

void SynchronousTaskGraphRunner::RunUntilIdle() {
  while (work_queue_.HasReadyToRunTasks())
    RunTask();
}

void SynchronousTaskGraphRunner::RunTask() {
  TRACE_EVENT0("toplevel", "SynchronousTaskGraphRunner::RunTask");

  auto prioritized_task = work_queue_.GetNextTaskToRun();

  Task* task = prioritized_task.task;
  task->WillRun();
  task->RunOnWorkerThread();
  task->DidRun();

  work_queue_.CompleteTask(prioritized_task);
}

}  // namespace cc
