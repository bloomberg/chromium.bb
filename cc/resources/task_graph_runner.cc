// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/task_graph_runner.h"

#include <algorithm>

#include "base/containers/hash_tables.h"
#include "base/debug/trace_event.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"

namespace cc {
namespace internal {

Task::Task() : did_schedule_(false), did_run_(false) {}

Task::~Task() { DCHECK(!did_run_ || did_schedule_); }

void Task::DidSchedule() { did_schedule_ = true; }

void Task::WillRun() {
  DCHECK(did_schedule_);
  DCHECK(!did_run_);
}

void Task::DidRun() { did_run_ = true; }

bool Task::HasFinishedRunning() const { return did_run_; }

GraphNode::GraphNode(Task* task, unsigned priority)
    : task_(task), priority_(priority), num_dependencies_(0) {}

GraphNode::~GraphNode() {}

TaskGraphRunner::TaskNamespace::TaskNamespace() {}

TaskGraphRunner::TaskNamespace::~TaskNamespace() {}

TaskGraphRunner::TaskGraphRunner(size_t num_threads,
                                 const std::string& thread_name_prefix)
    : lock_(),
      has_ready_to_run_tasks_cv_(&lock_),
      has_namespaces_with_finished_running_tasks_cv_(&lock_),
      next_namespace_id_(1),
      next_thread_index_(0u),
      shutdown_(false) {
  base::AutoLock lock(lock_);

  while (workers_.size() < num_threads) {
    scoped_ptr<base::DelegateSimpleThread> worker =
        make_scoped_ptr(new base::DelegateSimpleThread(
            this,
            thread_name_prefix +
                base::StringPrintf("Worker%u",
                                   static_cast<unsigned>(workers_.size() + 1))
                    .c_str()));
    worker->Start();
#if defined(OS_ANDROID) || defined(OS_LINUX)
    worker->SetThreadPriority(base::kThreadPriority_Background);
#endif
    workers_.push_back(worker.Pass());
  }
}

TaskGraphRunner::~TaskGraphRunner() {
  {
    base::AutoLock lock(lock_);

    DCHECK_EQ(0u, ready_to_run_namespaces_.size());
    DCHECK_EQ(0u, namespaces_.size());

    DCHECK(!shutdown_);
    shutdown_ = true;

    // Wake up a worker so it knows it should exit. This will cause all workers
    // to exit as each will wake up another worker before exiting.
    has_ready_to_run_tasks_cv_.Signal();
  }

  while (workers_.size()) {
    scoped_ptr<base::DelegateSimpleThread> worker = workers_.take_front();
    // Join() is considered IO and will block this thread.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    worker->Join();
  }
}

NamespaceToken TaskGraphRunner::GetNamespaceToken() {
  base::AutoLock lock(lock_);

  NamespaceToken token(next_namespace_id_++);
  DCHECK(namespaces_.find(token.id_) == namespaces_.end());
  return token;
}

void TaskGraphRunner::WaitForTasksToFinishRunning(NamespaceToken token) {
  base::AutoLock lock(lock_);

  DCHECK(token.IsValid());
  TaskNamespaceMap::iterator it = namespaces_.find(token.id_);
  if (it == namespaces_.end())
    return;

  TaskNamespace* task_namespace = it->second;
  while (!HasFinishedRunningTasksInNamespace(task_namespace))
    has_namespaces_with_finished_running_tasks_cv_.Wait();

  // There may be other namespaces that have finished running
  // tasks, so wake up another origin thread.
  has_namespaces_with_finished_running_tasks_cv_.Signal();
}

void TaskGraphRunner::SetTaskGraph(NamespaceToken token, TaskGraph* graph) {
  DCHECK(token.IsValid());

  TaskGraph new_pending_tasks;
  TaskGraph new_running_tasks;

  new_pending_tasks.swap(*graph);

  {
    base::AutoLock lock(lock_);

    DCHECK(!shutdown_);

    scoped_ptr<TaskNamespace> task_namespace =
        namespaces_.take_and_erase(token.id_);

    // Create task namespace if it doesn't exist.
    if (!task_namespace)
      task_namespace.reset(new TaskNamespace);

    // First remove all completed tasks from |new_pending_tasks| and
    // adjust number of dependencies.
    for (Task::Vector::iterator it = task_namespace->completed_tasks.begin();
         it != task_namespace->completed_tasks.end();
         ++it) {
      Task* task = it->get();

      scoped_ptr<GraphNode> node = new_pending_tasks.take_and_erase(task);
      if (node) {
        for (GraphNode::Vector::const_iterator it = node->dependents().begin();
             it != node->dependents().end();
             ++it) {
          GraphNode* dependent_node = *it;
          dependent_node->remove_dependency();
        }
      }
    }

    // Build new running task set.
    for (TaskGraph::iterator it = task_namespace->running_tasks.begin();
         it != task_namespace->running_tasks.end();
         ++it) {
      Task* task = it->first;
      // Transfer scheduled task value from |new_pending_tasks| to
      // |new_running_tasks| if currently running. Value must be set to
      // NULL if |new_pending_tasks| doesn't contain task. This does
      // the right in both cases.
      new_running_tasks.set(task, new_pending_tasks.take_and_erase(task));
    }

    // Build new "ready to run" tasks queue.
    task_namespace->ready_to_run_tasks.clear();
    for (TaskGraph::iterator it = new_pending_tasks.begin();
         it != new_pending_tasks.end();
         ++it) {
      Task* task = it->first;
      DCHECK(task);
      GraphNode* node = it->second;

      // Completed tasks should not exist in |new_pending_tasks|.
      DCHECK(!task->HasFinishedRunning());

      // Call DidSchedule() to indicate that this task has been scheduled.
      // Note: This is only for debugging purposes.
      task->DidSchedule();

      if (!node->num_dependencies())
        task_namespace->ready_to_run_tasks.push_back(node);

      // Erase the task from old pending tasks.
      task_namespace->pending_tasks.erase(task);
    }

    // Rearrange the elements in |ready_to_run_tasks| in such a way that
    // they form a heap.
    std::make_heap(task_namespace->ready_to_run_tasks.begin(),
                   task_namespace->ready_to_run_tasks.end(),
                   CompareTaskPriority);

    task_namespace->completed_tasks.reserve(
        task_namespace->completed_tasks.size() +
        task_namespace->pending_tasks.size());

    // The items left in |pending_tasks| need to be canceled.
    for (TaskGraph::const_iterator it = task_namespace->pending_tasks.begin();
         it != task_namespace->pending_tasks.end();
         ++it) {
      task_namespace->completed_tasks.push_back(it->first);
    }

    // Swap task sets.
    // Note: old tasks are intentionally destroyed after releasing |lock_|.
    task_namespace->pending_tasks.swap(new_pending_tasks);
    task_namespace->running_tasks.swap(new_running_tasks);

    // If |ready_to_run_tasks| is empty, it means we either have
    // running tasks, or we have no pending tasks.
    DCHECK(!task_namespace->ready_to_run_tasks.empty() ||
           (task_namespace->pending_tasks.empty() ||
            !task_namespace->running_tasks.empty()));

    // Add task namespace if not empty.
    if (!task_namespace->pending_tasks.empty() ||
        !task_namespace->running_tasks.empty() ||
        !task_namespace->completed_tasks.empty()) {
      namespaces_.set(token.id_, task_namespace.Pass());
    }

    // Build new "ready to run" task namespaces queue.
    ready_to_run_namespaces_.clear();
    for (TaskNamespaceMap::iterator it = namespaces_.begin();
         it != namespaces_.end();
         ++it) {
      if (!it->second->ready_to_run_tasks.empty())
        ready_to_run_namespaces_.push_back(it->second);
    }

    // Rearrange the task namespaces in |ready_to_run_namespaces_|
    // in such a way that they form a heap.
    std::make_heap(ready_to_run_namespaces_.begin(),
                   ready_to_run_namespaces_.end(),
                   CompareTaskNamespacePriority);

    // If there is more work available, wake up worker thread.
    if (!ready_to_run_namespaces_.empty())
      has_ready_to_run_tasks_cv_.Signal();
  }
}

void TaskGraphRunner::CollectCompletedTasks(NamespaceToken token,
                                            Task::Vector* completed_tasks) {
  base::AutoLock lock(lock_);

  DCHECK(token.IsValid());
  TaskNamespaceMap::iterator it = namespaces_.find(token.id_);
  if (it == namespaces_.end())
    return;

  TaskNamespace* task_namespace = it->second;

  DCHECK_EQ(0u, completed_tasks->size());
  completed_tasks->swap(task_namespace->completed_tasks);
  if (!HasFinishedRunningTasksInNamespace(task_namespace))
    return;

  // Remove namespace if finished running tasks.
  DCHECK_EQ(0u, task_namespace->pending_tasks.size());
  DCHECK_EQ(0u, task_namespace->running_tasks.size());
  DCHECK_EQ(0u, task_namespace->completed_tasks.size());
  DCHECK_EQ(0u, task_namespace->ready_to_run_tasks.size());
  namespaces_.erase(it);
}

void TaskGraphRunner::Run() {
  base::AutoLock lock(lock_);

  // Get a unique thread index.
  int thread_index = next_thread_index_++;

  while (true) {
    if (ready_to_run_namespaces_.empty()) {
      // Exit when shutdown is set and no more tasks are pending.
      if (shutdown_)
        break;

      // Wait for more tasks.
      has_ready_to_run_tasks_cv_.Wait();
      continue;
    }

    // Take top priority TaskNamespace from |ready_to_run_namespaces_|.
    std::pop_heap(ready_to_run_namespaces_.begin(),
                  ready_to_run_namespaces_.end(),
                  CompareTaskNamespacePriority);
    TaskNamespace* task_namespace = ready_to_run_namespaces_.back();
    ready_to_run_namespaces_.pop_back();
    DCHECK(!task_namespace->ready_to_run_tasks.empty());

    // Take top priority task from |ready_to_run_tasks|.
    std::pop_heap(task_namespace->ready_to_run_tasks.begin(),
                  task_namespace->ready_to_run_tasks.end(),
                  CompareTaskPriority);
    scoped_refptr<Task> task(task_namespace->ready_to_run_tasks.back()->task());
    task_namespace->ready_to_run_tasks.pop_back();

    // Add task namespace back to |ready_to_run_namespaces_| if not
    // empty after taking top priority task.
    if (!task_namespace->ready_to_run_tasks.empty()) {
      ready_to_run_namespaces_.push_back(task_namespace);
      std::push_heap(ready_to_run_namespaces_.begin(),
                     ready_to_run_namespaces_.end(),
                     CompareTaskNamespacePriority);
    }

    // Move task from |pending_tasks| to |running_tasks|.
    DCHECK(task_namespace->pending_tasks.contains(task.get()));
    DCHECK(!task_namespace->running_tasks.contains(task.get()));
    task_namespace->running_tasks.set(
        task.get(), task_namespace->pending_tasks.take_and_erase(task.get()));

    // There may be more work available, so wake up another worker thread.
    has_ready_to_run_tasks_cv_.Signal();

    // Call WillRun() before releasing |lock_| and running task.
    task->WillRun();

    {
      base::AutoUnlock unlock(lock_);

      task->RunOnWorkerThread(thread_index);
    }

    // This will mark task as finished running.
    task->DidRun();

    // Now iterate over all dependents to remove dependency and check
    // if they are ready to run.
    scoped_ptr<GraphNode> node =
        task_namespace->running_tasks.take_and_erase(task.get());
    if (node) {
      bool ready_to_run_namespaces_has_heap_properties = true;

      for (GraphNode::Vector::const_iterator it = node->dependents().begin();
           it != node->dependents().end();
           ++it) {
        GraphNode* dependent_node = *it;

        dependent_node->remove_dependency();
        // Task is ready if it has no dependencies. Add it to
        // |ready_to_run_tasks_|.
        if (!dependent_node->num_dependencies()) {
          bool was_empty = task_namespace->ready_to_run_tasks.empty();
          task_namespace->ready_to_run_tasks.push_back(dependent_node);
          std::push_heap(task_namespace->ready_to_run_tasks.begin(),
                         task_namespace->ready_to_run_tasks.end(),
                         CompareTaskPriority);
          // Task namespace is ready if it has at least one ready
          // to run task. Add it to |ready_to_run_namespaces_| if
          // it just become ready.
          if (was_empty) {
            DCHECK(std::find(ready_to_run_namespaces_.begin(),
                             ready_to_run_namespaces_.end(),
                             task_namespace) == ready_to_run_namespaces_.end());
            ready_to_run_namespaces_.push_back(task_namespace);
          }
          ready_to_run_namespaces_has_heap_properties = false;
        }
      }

      // Rearrange the task namespaces in |ready_to_run_namespaces_|
      // in such a way that they yet again form a heap.
      if (!ready_to_run_namespaces_has_heap_properties) {
        std::make_heap(ready_to_run_namespaces_.begin(),
                       ready_to_run_namespaces_.end(),
                       CompareTaskNamespacePriority);
      }
    }

    // Finally add task to |completed_tasks_|.
    task_namespace->completed_tasks.push_back(task);

    // If namespace has finished running all tasks, wake up origin thread.
    if (HasFinishedRunningTasksInNamespace(task_namespace))
      has_namespaces_with_finished_running_tasks_cv_.Signal();
  }

  // We noticed we should exit. Wake up the next worker so it knows it should
  // exit as well (because the Shutdown() code only signals once).
  has_ready_to_run_tasks_cv_.Signal();
}

}  // namespace internal
}  // namespace cc
