// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/worker_pool.h"

#include <algorithm>
#include <queue>

#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/debug/trace_event.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/condition_variable.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_restrictions.h"
#include "cc/base/scoped_ptr_deque.h"

namespace cc {

namespace internal {

WorkerPoolTask::WorkerPoolTask()
    : did_schedule_(false),
      did_run_(false),
      did_complete_(false) {
}

WorkerPoolTask::~WorkerPoolTask() {
  DCHECK_EQ(did_schedule_, did_complete_);
  DCHECK(!did_run_ || did_schedule_);
  DCHECK(!did_run_ || did_complete_);
}

void WorkerPoolTask::DidSchedule() {
  DCHECK(!did_complete_);
  did_schedule_ = true;
}

void WorkerPoolTask::WillRun() {
  DCHECK(did_schedule_);
  DCHECK(!did_complete_);
  DCHECK(!did_run_);
}

void WorkerPoolTask::DidRun() {
  did_run_ = true;
}

void WorkerPoolTask::WillComplete() {
  DCHECK(!did_complete_);
}

void WorkerPoolTask::DidComplete() {
  DCHECK(did_schedule_);
  DCHECK(!did_complete_);
  did_complete_ = true;
}

bool WorkerPoolTask::HasFinishedRunning() const {
  return did_run_;
}

bool WorkerPoolTask::HasCompleted() const {
  return did_complete_;
}

GraphNode::GraphNode(internal::WorkerPoolTask* task, unsigned priority)
    : task_(task),
      priority_(priority),
      num_dependencies_(0) {
}

GraphNode::~GraphNode() {
}

}  // namespace internal

// Internal to the worker pool. Any data or logic that needs to be
// shared between threads lives in this class. All members are guarded
// by |lock_|.
class WorkerPool::Inner : public base::DelegateSimpleThread::Delegate {
 public:
  Inner(size_t num_threads, const std::string& thread_name_prefix);
  virtual ~Inner();

  void Shutdown();

  // Schedule running of tasks in |graph|. Tasks previously scheduled but
  // no longer needed will be canceled unless already running. Canceled
  // tasks are moved to |completed_tasks_| without being run. The result
  // is that once scheduled, a task is guaranteed to end up in the
  // |completed_tasks_| queue even if they later get canceled by another
  // call to SetTaskGraph().
  void SetTaskGraph(TaskGraph* graph);

  // Collect all completed tasks in |completed_tasks|.
  void CollectCompletedTasks(TaskVector* completed_tasks);

 private:
  class PriorityComparator {
   public:
    bool operator()(const internal::GraphNode* a,
                    const internal::GraphNode* b) {
      // In this system, numerically lower priority is run first.
      if (a->priority() != b->priority())
        return a->priority() > b->priority();

      // Run task with most dependents first when priority is the same.
      return a->dependents().size() < b->dependents().size();
    }
  };

  // Overridden from base::DelegateSimpleThread:
  virtual void Run() OVERRIDE;

  // This lock protects all members of this class except
  // |worker_pool_on_origin_thread_|. Do not read or modify anything
  // without holding this lock. Do not block while holding this lock.
  mutable base::Lock lock_;

  // Condition variable that is waited on by worker threads until new
  // tasks are ready to run or shutdown starts.
  base::ConditionVariable has_ready_to_run_tasks_cv_;

  // Provides each running thread loop with a unique index. First thread
  // loop index is 0.
  unsigned next_thread_index_;

  // Set during shutdown. Tells workers to exit when no more tasks
  // are pending.
  bool shutdown_;

  // This set contains all pending tasks.
  GraphNodeMap pending_tasks_;

  // Ordered set of tasks that are ready to run.
  typedef std::priority_queue<internal::GraphNode*,
                              std::vector<internal::GraphNode*>,
                              PriorityComparator> TaskQueue;
  TaskQueue ready_to_run_tasks_;

  // This set contains all currently running tasks.
  GraphNodeMap running_tasks_;

  // Completed tasks not yet collected by origin thread.
  TaskVector completed_tasks_;

  ScopedPtrDeque<base::DelegateSimpleThread> workers_;

  DISALLOW_COPY_AND_ASSIGN(Inner);
};

WorkerPool::Inner::Inner(
    size_t num_threads, const std::string& thread_name_prefix)
    : lock_(),
      has_ready_to_run_tasks_cv_(&lock_),
      next_thread_index_(0),
      shutdown_(false) {
  base::AutoLock lock(lock_);

  while (workers_.size() < num_threads) {
    scoped_ptr<base::DelegateSimpleThread> worker = make_scoped_ptr(
        new base::DelegateSimpleThread(
            this,
            thread_name_prefix +
            base::StringPrintf(
                "Worker%u",
                static_cast<unsigned>(workers_.size() + 1)).c_str()));
    worker->Start();
#if defined(OS_ANDROID) || defined(OS_LINUX)
    worker->SetThreadPriority(base::kThreadPriority_Background);
#endif
    workers_.push_back(worker.Pass());
  }
}

WorkerPool::Inner::~Inner() {
  base::AutoLock lock(lock_);

  DCHECK(shutdown_);

  DCHECK_EQ(0u, pending_tasks_.size());
  DCHECK_EQ(0u, ready_to_run_tasks_.size());
  DCHECK_EQ(0u, running_tasks_.size());
  DCHECK_EQ(0u, completed_tasks_.size());
}

void WorkerPool::Inner::Shutdown() {
  {
    base::AutoLock lock(lock_);

    DCHECK(!shutdown_);
    shutdown_ = true;

    // Wake up a worker so it knows it should exit. This will cause all workers
    // to exit as each will wake up another worker before exiting.
    has_ready_to_run_tasks_cv_.Signal();
  }

  while (workers_.size()) {
    scoped_ptr<base::DelegateSimpleThread> worker = workers_.take_front();
    // http://crbug.com/240453 - Join() is considered IO and will block this
    // thread. See also http://crbug.com/239423 for further ideas.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    worker->Join();
  }
}

void WorkerPool::Inner::SetTaskGraph(TaskGraph* graph) {
  // It is OK to call SetTaskGraph() after shutdown if |graph| is empty.
  DCHECK(graph->empty() || !shutdown_);

  GraphNodeMap new_pending_tasks;
  GraphNodeMap new_running_tasks;
  TaskQueue new_ready_to_run_tasks;

  new_pending_tasks.swap(*graph);

  {
    base::AutoLock lock(lock_);

    // First remove all completed tasks from |new_pending_tasks| and
    // adjust number of dependencies.
    for (TaskVector::iterator it = completed_tasks_.begin();
         it != completed_tasks_.end(); ++it) {
      internal::WorkerPoolTask* task = it->get();

      scoped_ptr<internal::GraphNode> node = new_pending_tasks.take_and_erase(
          task);
      if (node) {
        for (internal::GraphNode::Vector::const_iterator it =
                 node->dependents().begin();
             it != node->dependents().end(); ++it) {
          internal::GraphNode* dependent_node = *it;
          dependent_node->remove_dependency();
        }
      }
    }

    // Build new running task set.
    for (GraphNodeMap::iterator it = running_tasks_.begin();
         it != running_tasks_.end(); ++it) {
      internal::WorkerPoolTask* task = it->first;
      // Transfer scheduled task value from |new_pending_tasks| to
      // |new_running_tasks| if currently running. Value must be set to
      // NULL if |new_pending_tasks| doesn't contain task. This does
      // the right in both cases.
      new_running_tasks.set(task, new_pending_tasks.take_and_erase(task));
    }

    // Build new "ready to run" tasks queue.
    // TODO(reveman): Create this queue when building the task graph instead.
    for (GraphNodeMap::iterator it = new_pending_tasks.begin();
         it != new_pending_tasks.end(); ++it) {
      internal::WorkerPoolTask* task = it->first;
      DCHECK(task);
      internal::GraphNode* node = it->second;

      // Completed tasks should not exist in |new_pending_tasks|.
      DCHECK(!task->HasFinishedRunning());

      // Call DidSchedule() to indicate that this task has been scheduled.
      // Note: This is only for debugging purposes.
      task->DidSchedule();

      if (!node->num_dependencies())
        new_ready_to_run_tasks.push(node);

      // Erase the task from old pending tasks.
      pending_tasks_.erase(task);
    }

    completed_tasks_.reserve(completed_tasks_.size() + pending_tasks_.size());

    // The items left in |pending_tasks_| need to be canceled.
    for (GraphNodeMap::const_iterator it = pending_tasks_.begin();
         it != pending_tasks_.end();
         ++it) {
      completed_tasks_.push_back(it->first);
    }

    // Swap task sets.
    // Note: old tasks are intentionally destroyed after releasing |lock_|.
    pending_tasks_.swap(new_pending_tasks);
    running_tasks_.swap(new_running_tasks);
    std::swap(ready_to_run_tasks_, new_ready_to_run_tasks);

    // If |ready_to_run_tasks_| is empty, it means we either have
    // running tasks, or we have no pending tasks.
    DCHECK(!ready_to_run_tasks_.empty() ||
           (pending_tasks_.empty() || !running_tasks_.empty()));

    // If there is more work available, wake up worker thread.
    if (!ready_to_run_tasks_.empty())
      has_ready_to_run_tasks_cv_.Signal();
  }
}

void WorkerPool::Inner::CollectCompletedTasks(TaskVector* completed_tasks) {
  base::AutoLock lock(lock_);

  DCHECK_EQ(0u, completed_tasks->size());
  completed_tasks->swap(completed_tasks_);
}

void WorkerPool::Inner::Run() {
  base::AutoLock lock(lock_);

  // Get a unique thread index.
  int thread_index = next_thread_index_++;

  while (true) {
    if (ready_to_run_tasks_.empty()) {
      // Exit when shutdown is set and no more tasks are pending.
      if (shutdown_ && pending_tasks_.empty())
        break;

      // Wait for more tasks.
      has_ready_to_run_tasks_cv_.Wait();
      continue;
    }

    // Take top priority task from |ready_to_run_tasks_|.
    scoped_refptr<internal::WorkerPoolTask> task(
        ready_to_run_tasks_.top()->task());
    ready_to_run_tasks_.pop();

    // Move task from |pending_tasks_| to |running_tasks_|.
    DCHECK(pending_tasks_.contains(task.get()));
    DCHECK(!running_tasks_.contains(task.get()));
    running_tasks_.set(task.get(), pending_tasks_.take_and_erase(task.get()));

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
    scoped_ptr<internal::GraphNode> node = running_tasks_.take_and_erase(
        task.get());
    if (node) {
      for (internal::GraphNode::Vector::const_iterator it =
               node->dependents().begin();
           it != node->dependents().end(); ++it) {
        internal::GraphNode* dependent_node = *it;

        dependent_node->remove_dependency();
        // Task is ready if it has no dependencies. Add it to
        // |ready_to_run_tasks_|.
        if (!dependent_node->num_dependencies())
          ready_to_run_tasks_.push(dependent_node);
      }
    }

    // Finally add task to |completed_tasks_|.
    completed_tasks_.push_back(task);
  }

  // We noticed we should exit. Wake up the next worker so it knows it should
  // exit as well (because the Shutdown() code only signals once).
  has_ready_to_run_tasks_cv_.Signal();
}

WorkerPool::WorkerPool(size_t num_threads,
                       const std::string& thread_name_prefix)
    : in_dispatch_completion_callbacks_(false),
      inner_(make_scoped_ptr(new Inner(num_threads, thread_name_prefix))) {
}

WorkerPool::~WorkerPool() {
}

void WorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "WorkerPool::Shutdown");

  DCHECK(!in_dispatch_completion_callbacks_);

  inner_->Shutdown();
}

void WorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "WorkerPool::CheckForCompletedTasks");

  DCHECK(!in_dispatch_completion_callbacks_);

  TaskVector completed_tasks;
  inner_->CollectCompletedTasks(&completed_tasks);
  ProcessCompletedTasks(completed_tasks);
}

void WorkerPool::ProcessCompletedTasks(
    const TaskVector& completed_tasks) {
  TRACE_EVENT1("cc", "WorkerPool::ProcessCompletedTasks",
               "completed_task_count", completed_tasks.size());

  // Worker pool instance is not reentrant while processing completed tasks.
  in_dispatch_completion_callbacks_ = true;

  for (TaskVector::const_iterator it = completed_tasks.begin();
       it != completed_tasks.end();
       ++it) {
    internal::WorkerPoolTask* task = it->get();

    task->WillComplete();
    task->CompleteOnOriginThread();
    task->DidComplete();
  }

  in_dispatch_completion_callbacks_ = false;
}

void WorkerPool::SetTaskGraph(TaskGraph* graph) {
  TRACE_EVENT1("cc", "WorkerPool::SetTaskGraph",
               "num_tasks", graph->size());

  DCHECK(!in_dispatch_completion_callbacks_);

  inner_->SetTaskGraph(graph);
}

}  // namespace cc
