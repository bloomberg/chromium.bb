// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/worker_pool.h"

#if defined(OS_ANDROID)
// TODO(epenner): Move thread priorities to base. (crbug.com/170549)
#include <sys/resource.h>
#endif

#include <map>

#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/debug/trace_event.h"
#include "base/strings/stringprintf.h"
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

WorkerPoolTask::WorkerPoolTask(TaskVector* dependencies)
    : did_schedule_(false),
      did_run_(false),
      did_complete_(false) {
  dependencies_.swap(*dependencies);
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

void WorkerPoolTask::DidComplete() {
  DCHECK(did_schedule_);
  DCHECK(!did_complete_);
  did_complete_ = true;
}

bool WorkerPoolTask::IsReadyToRun() const {
  // TODO(reveman): Use counter to improve performance.
  for (TaskVector::const_reverse_iterator it = dependencies_.rbegin();
       it != dependencies_.rend(); ++it) {
    WorkerPoolTask* dependency = it->get();
    if (!dependency->HasFinishedRunning())
      return false;
  }
  return true;
}

bool WorkerPoolTask::HasFinishedRunning() const {
  return did_run_;
}

bool WorkerPoolTask::HasCompleted() const {
  return did_complete_;
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
  void CollectCompletedTasks(TaskDeque* completed_tasks);

 private:
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
  // TODO(reveman): priority_queue might be more efficient.
  typedef std::map<unsigned, internal::WorkerPoolTask*> TaskMap;
  TaskMap ready_to_run_tasks_;

  // This set contains all currently running tasks.
  GraphNodeMap running_tasks_;

  // Completed tasks not yet collected by origin thread.
  TaskDeque completed_tasks_;

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
  TaskMap new_ready_to_run_tasks;

  new_pending_tasks.swap(*graph);

  {
    base::AutoLock lock(lock_);

    // First remove all completed tasks from |new_pending_tasks|.
    for (TaskDeque::iterator it = completed_tasks_.begin();
         it != completed_tasks_.end(); ++it) {
      internal::WorkerPoolTask* task = it->get();
      new_pending_tasks.take_and_erase(task);
    }

    // Move tasks not present in |new_pending_tasks| to |completed_tasks_|.
    for (GraphNodeMap::iterator it = pending_tasks_.begin();
         it != pending_tasks_.end(); ++it) {
      internal::WorkerPoolTask* task = it->first;

      // Task has completed if not present in |new_pending_tasks|.
      if (!new_pending_tasks.contains(task))
        completed_tasks_.push_back(task);
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
    for (GraphNodeMap::iterator it = new_pending_tasks.begin();
         it != new_pending_tasks.end(); ++it) {
      internal::WorkerPoolTask* task = it->first;

      // Completed tasks should not exist in |new_pending_tasks|.
      DCHECK(!task->HasFinishedRunning());

      // Call DidSchedule() to indicate that this task has been scheduled.
      // Note: This is only for debugging purposes.
      task->DidSchedule();

      DCHECK_EQ(0u, new_ready_to_run_tasks.count(it->second->priority()));
      if (task->IsReadyToRun())
        new_ready_to_run_tasks[it->second->priority()] = task;
    }

    // Swap task sets.
    // Note: old tasks are intentionally destroyed after releasing |lock_|.
    pending_tasks_.swap(new_pending_tasks);
    running_tasks_.swap(new_running_tasks);
    ready_to_run_tasks_.swap(new_ready_to_run_tasks);

    // If there is more work available, wake up worker thread.
    if (!ready_to_run_tasks_.empty())
      has_ready_to_run_tasks_cv_.Signal();
  }
}

void WorkerPool::Inner::CollectCompletedTasks(TaskDeque* completed_tasks) {
  base::AutoLock lock(lock_);

  DCHECK_EQ(0u, completed_tasks->size());
  completed_tasks->swap(completed_tasks_);
}

void WorkerPool::Inner::Run() {
#if defined(OS_ANDROID)
  base::PlatformThread::SetThreadPriority(
      base::PlatformThread::CurrentHandle(),
      base::kThreadPriority_Background);
#endif

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
        ready_to_run_tasks_.begin()->second);
    ready_to_run_tasks_.erase(ready_to_run_tasks_.begin());

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

      task->RunOnThread(thread_index);
    }

    // This will mark task as finished running.
    task->DidRun();

    // Now iterate over all dependents to check if they are ready to run.
    scoped_ptr<GraphNode> node = running_tasks_.take_and_erase(task.get());
    if (node) {
      typedef internal::WorkerPoolTask::TaskVector TaskVector;
      for (TaskVector::const_iterator it = node->dependents().begin();
           it != node->dependents().end(); ++it) {
        GraphNodeMap::iterator dependent_it = pending_tasks_.find(it->get());
        DCHECK(dependent_it != pending_tasks_.end());

        internal::WorkerPoolTask* dependent = dependent_it->first;
        if (!dependent->IsReadyToRun())
          continue;

        // Task is ready. Add it to |ready_to_run_tasks_|.
        GraphNode* dependent_node = dependent_it->second;
        unsigned priority = dependent_node->priority();
        DCHECK(!ready_to_run_tasks_.count(priority) ||
               ready_to_run_tasks_[priority] == dependent);
        ready_to_run_tasks_[priority] = dependent;
      }
    }

    // Finally add task to |completed_tasks_|.
    completed_tasks_.push_back(task);
  }

  // We noticed we should exit. Wake up the next worker so it knows it should
  // exit as well (because the Shutdown() code only signals once).
  has_ready_to_run_tasks_cv_.Signal();
}

WorkerPool::GraphNode::GraphNode(
    internal::WorkerPoolTask* dependent, unsigned priority)
    : priority_(priority) {
  if (dependent)
    dependents_.push_back(dependent);
}

WorkerPool::GraphNode::~GraphNode() {
}

void WorkerPool::GraphNode::AddDependent(internal::WorkerPoolTask* dependent) {
  DCHECK(dependent);
  dependents_.push_back(dependent);
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

  TaskDeque completed_tasks;
  inner_->CollectCompletedTasks(&completed_tasks);
  DispatchCompletionCallbacks(&completed_tasks);
}

void WorkerPool::DispatchCompletionCallbacks(TaskDeque* completed_tasks) {
  TRACE_EVENT0("cc", "WorkerPool::DispatchCompletionCallbacks");

  // Worker pool instance is not reentrant while processing completed tasks.
  in_dispatch_completion_callbacks_ = true;

  while (!completed_tasks->empty()) {
    internal::WorkerPoolTask* task = completed_tasks->front().get();

    task->DidComplete();
    task->DispatchCompletionCallback();

    completed_tasks->pop_front();
  }

  in_dispatch_completion_callbacks_ = false;
}

void WorkerPool::SetTaskGraph(TaskGraph* graph) {
  TRACE_EVENT0("cc", "WorkerPool::SetTaskGraph");

  DCHECK(!in_dispatch_completion_callbacks_);

  inner_->SetTaskGraph(graph);
}

// static
unsigned WorkerPool::BuildTaskGraphRecursive(
    internal::WorkerPoolTask* task,
    internal::WorkerPoolTask* dependent,
    unsigned priority,
    TaskGraph* tasks) {
  // Skip sub-tree if task has already completed.
  if (task->HasCompleted())
    return priority;

  GraphNodeMap::iterator it = tasks->find(task);
  if (it != tasks->end()) {
    it->second->AddDependent(dependent);
    return priority;
  }

  typedef internal::WorkerPoolTask::TaskVector TaskVector;
  for (TaskVector::iterator dependency_it = task->dependencies().begin();
       dependency_it != task->dependencies().end(); ++dependency_it) {
    internal::WorkerPoolTask* dependency = dependency_it->get();
    priority = BuildTaskGraphRecursive(dependency, task, priority, tasks);
  }

  tasks->set(task, make_scoped_ptr(new GraphNode(dependent, priority)));

  return priority + 1;
}

// static
void WorkerPool::BuildTaskGraph(
    internal::WorkerPoolTask* root, TaskGraph* tasks) {
  const unsigned kBasePriority = 0u;
  if (root)
    BuildTaskGraphRecursive(root, NULL, kBasePriority, tasks);
}

}  // namespace cc
