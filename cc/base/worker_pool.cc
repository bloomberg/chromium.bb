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
#include "base/debug/trace_event.h"
#include "base/hash_tables.h"
#include "base/stringprintf.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_restrictions.h"
#include "cc/base/scoped_ptr_deque.h"
#include "cc/base/scoped_ptr_hash_map.h"

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template <> struct hash<cc::internal::WorkerPoolTask*> {
  size_t operator()(cc::internal::WorkerPoolTask* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
}  // namespace BASE_HASH_NAMESPACE
#endif  // COMPILER

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
  Inner(WorkerPool* worker_pool,
        size_t num_threads,
        const std::string& thread_name_prefix);
  virtual ~Inner();

  void Shutdown();

  // Schedule running of |root| task and all its dependencies. Tasks
  // previously scheduled but no longer needed to run |root| will be
  // canceled unless already running. Canceled tasks are moved to
  // |completed_tasks_| without being run. The result is that once
  // scheduled, a task is guaranteed to end up in the |completed_tasks_|
  // queue even if they later get canceled by another call to
  // ScheduleTasks().
  void ScheduleTasks(internal::WorkerPoolTask* root);

  // Collect all completed tasks in |completed_tasks|. Returns true if idle.
  bool CollectCompletedTasks(TaskDeque* completed_tasks);

 private:
  class ScheduledTask {
   public:
    ScheduledTask(internal::WorkerPoolTask* dependent, unsigned priority)
        : priority_(priority) {
      if (dependent)
        dependents_.push_back(dependent);
    }

    internal::WorkerPoolTask::TaskVector& dependents() { return dependents_; }
    unsigned priority() const { return priority_; }

   private:
    internal::WorkerPoolTask::TaskVector dependents_;
    unsigned priority_;
  };
  typedef internal::WorkerPoolTask* ScheduledTaskMapKey;
  typedef ScopedPtrHashMap<ScheduledTaskMapKey, ScheduledTask>
      ScheduledTaskMap;

  // This builds a ScheduledTaskMap from a root task.
  static unsigned BuildScheduledTaskMapRecursive(
      internal::WorkerPoolTask* task,
      internal::WorkerPoolTask* dependent,
      unsigned priority,
      ScheduledTaskMap* scheduled_tasks);
  static void BuildScheduledTaskMap(
      internal::WorkerPoolTask* root, ScheduledTaskMap* scheduled_tasks);

  // Collect all completed tasks by swapping the contents of
  // |completed_tasks| and |completed_tasks_|. Lock must be acquired
  // before calling this function. Returns true if idle.
  bool CollectCompletedTasksWithLockAcquired(TaskDeque* completed_tasks);

  // Schedule an OnIdleOnOriginThread callback if not already pending.
  // Lock must already be acquired before calling this function.
  void ScheduleOnIdleWithLockAcquired();
  void OnIdleOnOriginThread();

  // Overridden from base::DelegateSimpleThread:
  virtual void Run() OVERRIDE;

  // Pointer to worker pool. Can only be used on origin thread.
  // Not guarded by |lock_|.
  WorkerPool* worker_pool_on_origin_thread_;

  // This lock protects all members of this class except
  // |worker_pool_on_origin_thread_|. Do not read or modify anything
  // without holding this lock. Do not block while holding this lock.
  mutable base::Lock lock_;

  // Condition variable that is waited on by worker threads until new
  // tasks are ready to run or shutdown starts.
  base::ConditionVariable has_ready_to_run_tasks_cv_;

  // Target message loop used for posting callbacks.
  scoped_refptr<base::MessageLoopProxy> origin_loop_;

  base::WeakPtrFactory<Inner> weak_ptr_factory_;

  const base::Closure on_idle_callback_;
  // Set when a OnIdleOnOriginThread() callback is pending.
  bool on_idle_pending_;

  // Provides each running thread loop with a unique index. First thread
  // loop index is 0.
  unsigned next_thread_index_;

  // Set during shutdown. Tells workers to exit when no more tasks
  // are pending.
  bool shutdown_;

  // The root task that is a dependent of all other tasks.
  scoped_refptr<internal::WorkerPoolTask> root_;

  // This set contains all pending tasks.
  ScheduledTaskMap pending_tasks_;

  // Ordered set of tasks that are ready to run.
  // TODO(reveman): priority_queue might be more efficient.
  typedef std::map<unsigned, internal::WorkerPoolTask*> TaskMap;
  TaskMap ready_to_run_tasks_;

  // This set contains all currently running tasks.
  ScheduledTaskMap running_tasks_;

  // Completed tasks not yet collected by origin thread.
  TaskDeque completed_tasks_;

  ScopedPtrDeque<base::DelegateSimpleThread> workers_;

  DISALLOW_COPY_AND_ASSIGN(Inner);
};

WorkerPool::Inner::Inner(WorkerPool* worker_pool,
                         size_t num_threads,
                         const std::string& thread_name_prefix)
    : worker_pool_on_origin_thread_(worker_pool),
      lock_(),
      has_ready_to_run_tasks_cv_(&lock_),
      origin_loop_(base::MessageLoopProxy::current()),
      weak_ptr_factory_(this),
      on_idle_callback_(base::Bind(&WorkerPool::Inner::OnIdleOnOriginThread,
                                   weak_ptr_factory_.GetWeakPtr())),
      on_idle_pending_(false),
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

  // Cancel any pending OnIdle callback.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void WorkerPool::Inner::ScheduleTasks(internal::WorkerPoolTask* root) {
  // It is OK to call ScheduleTasks() after shutdown if |root| is NULL.
  DCHECK(!root || !shutdown_);

  scoped_refptr<internal::WorkerPoolTask> new_root(root);

  ScheduledTaskMap new_pending_tasks;
  ScheduledTaskMap new_running_tasks;
  TaskMap new_ready_to_run_tasks;

  // Build scheduled task map before acquiring |lock_|.
  if (root)
    BuildScheduledTaskMap(root, &new_pending_tasks);

  {
    base::AutoLock lock(lock_);

    // First remove all completed tasks from |new_pending_tasks|.
    for (TaskDeque::iterator it = completed_tasks_.begin();
         it != completed_tasks_.end(); ++it) {
      internal::WorkerPoolTask* task = it->get();
      new_pending_tasks.take_and_erase(task);
    }

    // Move tasks not present in |new_pending_tasks| to |completed_tasks_|.
    for (ScheduledTaskMap::iterator it = pending_tasks_.begin();
         it != pending_tasks_.end(); ++it) {
      internal::WorkerPoolTask* task = it->first;

      // Task has completed if not present in |new_pending_tasks|.
      if (!new_pending_tasks.contains(task))
        completed_tasks_.push_back(task);
    }

    // Build new running task set.
    for (ScheduledTaskMap::iterator it = running_tasks_.begin();
         it != running_tasks_.end(); ++it) {
      internal::WorkerPoolTask* task = it->first;
      // Transfer scheduled task value from |new_pending_tasks| to
      // |new_running_tasks| if currently running. Value must be set to
      // NULL if |new_pending_tasks| doesn't contain task. This does
      // the right in both cases.
      new_running_tasks.set(task, new_pending_tasks.take_and_erase(task));
    }

    // Build new "ready to run" tasks queue.
    for (ScheduledTaskMap::iterator it = new_pending_tasks.begin();
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

    // Swap root taskand task sets.
    // Note: old tasks are intentionally destroyed after releasing |lock_|.
    root_.swap(new_root);
    pending_tasks_.swap(new_pending_tasks);
    running_tasks_.swap(new_running_tasks);
    ready_to_run_tasks_.swap(new_ready_to_run_tasks);

    // If there is more work available, wake up worker thread.
    if (!ready_to_run_tasks_.empty())
      has_ready_to_run_tasks_cv_.Signal();
  }
}

bool WorkerPool::Inner::CollectCompletedTasks(TaskDeque* completed_tasks) {
  base::AutoLock lock(lock_);

  return CollectCompletedTasksWithLockAcquired(completed_tasks);
}

bool WorkerPool::Inner::CollectCompletedTasksWithLockAcquired(
    TaskDeque* completed_tasks) {
  lock_.AssertAcquired();

  DCHECK_EQ(0u, completed_tasks->size());
  completed_tasks->swap(completed_tasks_);

  return running_tasks_.empty() && pending_tasks_.empty();
}

void WorkerPool::Inner::ScheduleOnIdleWithLockAcquired() {
  lock_.AssertAcquired();

  if (on_idle_pending_)
    return;
  origin_loop_->PostTask(FROM_HERE, on_idle_callback_);
  on_idle_pending_ = true;
}

void WorkerPool::Inner::OnIdleOnOriginThread() {
  TaskDeque completed_tasks;

  {
    base::AutoLock lock(lock_);

    DCHECK(on_idle_pending_);
    on_idle_pending_ = false;

    // Early out if no longer idle.
    if (!running_tasks_.empty() || !pending_tasks_.empty())
      return;

    CollectCompletedTasksWithLockAcquired(&completed_tasks);
  }

  worker_pool_on_origin_thread_->OnIdle(&completed_tasks);
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
      if (pending_tasks_.empty()) {
        // Exit when shutdown is set and no more tasks are pending.
        if (shutdown_)
          break;

        // Schedule an idle callback if no tasks are running.
        if (running_tasks_.empty())
          ScheduleOnIdleWithLockAcquired();
      }

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
    scoped_ptr<ScheduledTask> scheduled_task =
        running_tasks_.take_and_erase(task.get());
    if (scheduled_task) {
      typedef internal::WorkerPoolTask::TaskVector TaskVector;
      for (TaskVector::iterator it = scheduled_task->dependents().begin();
           it != scheduled_task->dependents().end(); ++it) {
        internal::WorkerPoolTask* dependent = it->get();
        if (!dependent->IsReadyToRun())
          continue;

        // Task is ready. Add it to |ready_to_run_tasks_|.
        DCHECK(pending_tasks_.contains(dependent));
        unsigned priority = pending_tasks_.get(dependent)->priority();
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

// BuildScheduledTaskMap() takes a task tree as input and constructs
// a unique set of tasks with edges between dependencies pointing in
// the direction of the dependents. Each task is given a unique priority
// which is currently the same as the DFS traversal order.
//
// Input:             Output:
//
//       root               task4          Task | Priority (lower is better)
//     /      \           /       \      -------+---------------------------
//  task1    task2      task3    task2     root | 4
//    |        |          |        |      task1 | 2
//  task3      |        task1      |      task2 | 3
//    |        |           \      /       task3 | 1
//  task4    task4           root         task4 | 0
//
// The output can be used to efficiently maintain a queue of
// "ready to run" tasks.

// static
unsigned WorkerPool::Inner::BuildScheduledTaskMapRecursive(
    internal::WorkerPoolTask* task,
    internal::WorkerPoolTask* dependent,
    unsigned priority,
    ScheduledTaskMap* scheduled_tasks) {
  // Skip sub-tree if task has already completed.
  if (task->HasCompleted())
    return priority;

  ScheduledTaskMap::iterator scheduled_it = scheduled_tasks->find(task);
  if (scheduled_it != scheduled_tasks->end()) {
    DCHECK(dependent);
    scheduled_it->second->dependents().push_back(dependent);
    return priority;
  }

  typedef internal::WorkerPoolTask::TaskVector TaskVector;
  for (TaskVector::iterator it = task->dependencies().begin();
       it != task->dependencies().end(); ++it) {
    internal::WorkerPoolTask* dependency = it->get();
    priority = BuildScheduledTaskMapRecursive(
        dependency, task, priority, scheduled_tasks);
  }

  scheduled_tasks->set(task,
                       make_scoped_ptr(new ScheduledTask(dependent,
                                                         priority)));

  return priority + 1;
}

// static
void WorkerPool::Inner::BuildScheduledTaskMap(
    internal::WorkerPoolTask* root,
    ScheduledTaskMap* scheduled_tasks) {
  const unsigned kBasePriority = 0u;
  DCHECK(root);
  BuildScheduledTaskMapRecursive(root, NULL, kBasePriority, scheduled_tasks);
}

WorkerPool::WorkerPool(size_t num_threads,
                       base::TimeDelta check_for_completed_tasks_delay,
                       const std::string& thread_name_prefix)
    : client_(NULL),
      origin_loop_(base::MessageLoopProxy::current()),
      check_for_completed_tasks_delay_(check_for_completed_tasks_delay),
      check_for_completed_tasks_pending_(false),
      in_dispatch_completion_callbacks_(false),
      inner_(make_scoped_ptr(new Inner(this,
                                       num_threads,
                                       thread_name_prefix))) {
}

WorkerPool::~WorkerPool() {
}

void WorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "WorkerPool::Shutdown");

  DCHECK(!in_dispatch_completion_callbacks_);

  inner_->Shutdown();

  TaskDeque completed_tasks;
  inner_->CollectCompletedTasks(&completed_tasks);
  DispatchCompletionCallbacks(&completed_tasks);
}

void WorkerPool::OnIdle(TaskDeque* completed_tasks) {
  TRACE_EVENT0("cc", "WorkerPool::OnIdle");

  DCHECK(!in_dispatch_completion_callbacks_);

  DispatchCompletionCallbacks(completed_tasks);

  // Cancel any pending check for completed tasks.
  check_for_completed_tasks_callback_.Cancel();
  check_for_completed_tasks_pending_ = false;
}

void WorkerPool::ScheduleCheckForCompletedTasks() {
  if (check_for_completed_tasks_pending_)
    return;
  check_for_completed_tasks_callback_.Reset(
      base::Bind(&WorkerPool::CheckForCompletedTasks,
                 base::Unretained(this)));
  origin_loop_->PostDelayedTask(
      FROM_HERE,
      check_for_completed_tasks_callback_.callback(),
      check_for_completed_tasks_delay_);
  check_for_completed_tasks_pending_ = true;
}

void WorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "WorkerPool::CheckForCompletedTasks");

  DCHECK(!in_dispatch_completion_callbacks_);

  check_for_completed_tasks_callback_.Cancel();
  check_for_completed_tasks_pending_ = false;

  TaskDeque completed_tasks;

  // Schedule another check for completed tasks if not idle.
  if (!inner_->CollectCompletedTasks(&completed_tasks))
    ScheduleCheckForCompletedTasks();

  DispatchCompletionCallbacks(&completed_tasks);
}

void WorkerPool::DispatchCompletionCallbacks(TaskDeque* completed_tasks) {
  TRACE_EVENT0("cc", "WorkerPool::DispatchCompletionCallbacks");

  // Early out when |completed_tasks| is empty to prevent unnecessary
  // call to DidFinishDispatchingWorkerPoolCompletionCallbacks().
  if (completed_tasks->empty())
    return;

  // Worker pool instance is not reentrant while processing completed tasks.
  in_dispatch_completion_callbacks_ = true;

  while (!completed_tasks->empty()) {
    scoped_refptr<internal::WorkerPoolTask> task = completed_tasks->front();
    completed_tasks->pop_front();
    task->DidComplete();
    task->DispatchCompletionCallback();
  }

  in_dispatch_completion_callbacks_ = false;

  DCHECK(client_);
  client_->DidFinishDispatchingWorkerPoolCompletionCallbacks();
}

void WorkerPool::ScheduleTasks(internal::WorkerPoolTask* root) {
  TRACE_EVENT0("cc", "WorkerPool::ScheduleTasks");

  DCHECK(!in_dispatch_completion_callbacks_);

  // Schedule check for completed tasks.
  if (root)
    ScheduleCheckForCompletedTasks();

  inner_->ScheduleTasks(root);
}

}  // namespace cc
