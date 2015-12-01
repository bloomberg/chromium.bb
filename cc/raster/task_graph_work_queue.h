// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_TASK_GRAPH_WORK_QUEUE_H_
#define CC_RASTER_TASK_GRAPH_WORK_QUEUE_H_

#include <map>
#include <vector>

#include "cc/base/cc_export.h"
#include "cc/raster/task_graph_runner.h"

namespace cc {

// Implements a queue of incoming TaskGraph work. Designed for use by
// implementations of TaskGraphRunner. Not thread safe, so the caller is
// responsible for all necessary locking.
class CC_EXPORT TaskGraphWorkQueue {
 public:
  struct TaskNamespace;

  struct PrioritizedTask {
    typedef std::vector<PrioritizedTask> Vector;

    PrioritizedTask(Task* task, TaskNamespace* task_namespace, size_t priority)
        : task(task), task_namespace(task_namespace), priority(priority) {}

    Task* task;
    TaskNamespace* task_namespace;
    size_t priority;
  };

  // Helper classes and static methods used by dependent classes.
  struct TaskNamespace {
    typedef std::vector<TaskNamespace*> Vector;

    TaskNamespace();
    ~TaskNamespace();

    // Current task graph.
    TaskGraph graph;

    // Ordered set of tasks that are ready to run.
    PrioritizedTask::Vector ready_to_run_tasks;

    // Completed tasks not yet collected by origin thread.
    Task::Vector completed_tasks;

    // This set contains all currently running tasks.
    Task::Vector running_tasks;
  };

  TaskGraphWorkQueue();
  virtual ~TaskGraphWorkQueue();

  // Gets a NamespaceToken which is guaranteed to be unique within this
  // TaskGraphWorkQueue.
  NamespaceToken GetNamespaceToken();

  // Updates a TaskNamespace with a new TaskGraph to run. This cancels any
  // previous tasks in the graph being replaced.
  void ScheduleTasks(NamespaceToken token, TaskGraph* graph);

  // Returns the next task to run paired with its namespace.
  PrioritizedTask GetNextTaskToRun();

  // Marks a task as completed, adding it to its namespace's list of completed
  // tasks and updating the list of |ready_to_run_namespaces|.
  void CompleteTask(const PrioritizedTask& completed_task);

  // Helper which populates a vector of completed tasks from the provided
  // namespace.
  void CollectCompletedTasks(NamespaceToken token,
                             Task::Vector* completed_tasks);

  // Helper which returns the raw TaskNamespace* for the given token. Used to
  // allow callers to re-use a TaskNamespace*, reducing the number of lookups
  // needed.
  TaskNamespace* GetNamespaceForToken(NamespaceToken token) {
    auto it = namespaces_.find(token);
    if (it == namespaces_.end())
      return nullptr;
    return &it->second;
  }

  static bool HasFinishedRunningTasksInNamespace(
      const TaskNamespace* task_namespace) {
    return task_namespace->running_tasks.empty() &&
           task_namespace->ready_to_run_tasks.empty();
  }

  bool HasReadyToRunTasks() const { return !ready_to_run_namespaces_.empty(); }

  bool HasAnyNamespaces() const { return !namespaces_.empty(); }

  bool HasFinishedRunningTasksInAllNamespaces() {
    return std::find_if(
               namespaces_.begin(), namespaces_.end(),
               [](const TaskNamespaceMap::value_type& entry) {
                 return !HasFinishedRunningTasksInNamespace(&entry.second);
               }) == namespaces_.end();
  }

  // Helper function which ensures that graph dependencies were correctly
  // configured.
  static bool DependencyMismatch(const TaskGraph* graph);

 private:
  // Helper class used to provide NamespaceToken comparison to TaskNamespaceMap.
  class CompareToken {
   public:
    bool operator()(const NamespaceToken& lhs,
                    const NamespaceToken& rhs) const {
      return lhs.id_ < rhs.id_;
    }
  };

  static bool CompareTaskPriority(const PrioritizedTask& a,
                                  const PrioritizedTask& b) {
    // In this system, numerically lower priority is run first.
    return a.priority > b.priority;
  }

  static bool CompareTaskNamespacePriority(const TaskNamespace* a,
                                           const TaskNamespace* b) {
    DCHECK(!a->ready_to_run_tasks.empty());
    DCHECK(!b->ready_to_run_tasks.empty());

    // Compare based on task priority of the ready_to_run_tasks heap .front()
    // will hold the max element of the heap, except after pop_heap, when max
    // element is moved to .back().
    return CompareTaskPriority(a->ready_to_run_tasks.front(),
                               b->ready_to_run_tasks.front());
  }

  using TaskNamespaceMap =
      std::map<NamespaceToken, TaskNamespace, CompareToken>;

  TaskNamespaceMap namespaces_;
  TaskNamespace::Vector ready_to_run_namespaces_;

  // Provides a unique id to each NamespaceToken.
  int next_namespace_id_;
};

}  // namespace cc

#endif  // CC_RASTER_TASK_GRAPH_WORK_QUEUE_H_
