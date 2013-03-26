// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/worker_pool.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/stringprintf.h"
#include "base/synchronization/condition_variable.h"
#include "base/threading/simple_thread.h"

#if defined(OS_ANDROID)
// TODO(epenner): Move thread priorities to base. (crbug.com/170549)
#include <sys/resource.h>
#endif

namespace cc {

namespace {

class WorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  WorkerPoolTaskImpl(const WorkerPool::Callback& task,
                     const base::Closure& reply)
      : internal::WorkerPoolTask(reply),
        task_(task) {}

  virtual bool IsCheap() OVERRIDE { return false; }

  virtual void Run() OVERRIDE {
    task_.Run();
  }

  virtual void RunOnThread(unsigned thread_index) OVERRIDE {
    task_.Run();
  }

 private:
  WorkerPool::Callback task_;
};

}  // namespace

namespace internal {

WorkerPoolTask::WorkerPoolTask(const base::Closure& reply) : reply_(reply) {
}

WorkerPoolTask::~WorkerPoolTask() {
}

void WorkerPoolTask::DidComplete() {
  reply_.Run();
}

}  // namespace internal

// Internal to the worker pool. Any data or logic that needs to be
// shared between threads lives in this class. All members are guarded
// by |lock_|.
class WorkerPool::Inner : public base::DelegateSimpleThread::Delegate {
 public:
  Inner(WorkerPool* worker_pool,
        size_t num_threads,
        const std::string& thread_name_prefix,
        bool need_on_task_completed_callback);
  virtual ~Inner();

  void Shutdown();

  void PostTask(scoped_ptr<internal::WorkerPoolTask> task, bool signal_workers);

  // Appends all completed tasks to worker pool's completed tasks queue
  // and returns true if idle.
  bool CollectCompletedTasks();

  // Runs cheap tasks on caller thread until |time_limit| is reached
  // and returns true if idle.
  bool RunCheapTasksUntilTimeLimit(base::TimeTicks time_limit);

 private:
  // Appends all completed tasks to |completed_tasks|. Lock must
  // already be acquired before calling this function.
  bool AppendCompletedTasksWithLockAcquired(
      ScopedPtrDeque<internal::WorkerPoolTask>* completed_tasks);

  // Schedule a OnTaskCompletedOnOriginThread callback if not already
  // pending. Lock must already be acquired before calling this function.
  void ScheduleOnTaskCompletedWithLockAcquired();
  void OnTaskCompletedOnOriginThread();

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
  // tasks are posted or shutdown starts.
  base::ConditionVariable has_pending_tasks_cv_;

  // Target message loop used for posting callbacks.
  scoped_refptr<base::MessageLoopProxy> origin_loop_;

  base::WeakPtrFactory<Inner> weak_ptr_factory_;

  // Set to true when worker pool requires a callback for each
  // completed task.
  bool need_on_task_completed_callback_;

  const base::Closure on_task_completed_callback_;
  // Set when a OnTaskCompletedOnOriginThread() callback is pending.
  bool on_task_completed_pending_;

  const base::Closure on_idle_callback_;
  // Set when a OnIdleOnOriginThread() callback is pending.
  bool on_idle_pending_;

  // Provides each running thread loop with a unique index. First thread
  // loop index is 0.
  unsigned next_thread_index_;

  // Number of tasks currently running.
  unsigned running_task_count_;

  // Set during shutdown. Tells workers to exit when no more tasks
  // are pending.
  bool shutdown_;

  typedef ScopedPtrDeque<internal::WorkerPoolTask> TaskDeque;
  TaskDeque pending_tasks_;
  TaskDeque completed_tasks_;

  ScopedPtrDeque<base::DelegateSimpleThread> workers_;

  DISALLOW_COPY_AND_ASSIGN(Inner);
};

WorkerPool::Inner::Inner(WorkerPool* worker_pool,
                         size_t num_threads,
                         const std::string& thread_name_prefix,
                         bool need_on_task_completed_callback)
    : worker_pool_on_origin_thread_(worker_pool),
      lock_(),
      has_pending_tasks_cv_(&lock_),
      origin_loop_(base::MessageLoopProxy::current()),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      need_on_task_completed_callback_(need_on_task_completed_callback),
      on_task_completed_callback_(
          base::Bind(&WorkerPool::Inner::OnTaskCompletedOnOriginThread,
                     weak_ptr_factory_.GetWeakPtr())),
      on_task_completed_pending_(false),
      on_idle_callback_(base::Bind(&WorkerPool::Inner::OnIdleOnOriginThread,
                                   weak_ptr_factory_.GetWeakPtr())),
      on_idle_pending_(false),
      next_thread_index_(0),
      running_task_count_(0),
      shutdown_(false) {
  base::AutoLock lock(lock_);

  while (workers_.size() < num_threads) {
    scoped_ptr<base::DelegateSimpleThread> worker = make_scoped_ptr(
        new base::DelegateSimpleThread(
          this,
          thread_name_prefix +
          base::StringPrintf("Worker%lu", workers_.size() + 1).c_str()));
    worker->Start();
    workers_.push_back(worker.Pass());
  }
}

WorkerPool::Inner::~Inner() {
  base::AutoLock lock(lock_);

  DCHECK(shutdown_);

  // Cancel all pending callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  DCHECK_EQ(pending_tasks_.size(), 0);
  DCHECK_EQ(completed_tasks_.size(), 0);
  DCHECK_EQ(running_task_count_, 0);
}

void WorkerPool::Inner::Shutdown() {
  {
    base::AutoLock lock(lock_);

    DCHECK(!shutdown_);
    shutdown_ = true;

    // Wake up a worker so it knows it should exit. This will cause all workers
    // to exit as each will wake up another worker before exiting.
    has_pending_tasks_cv_.Signal();
  }

  while (workers_.size()) {
    scoped_ptr<base::DelegateSimpleThread> worker = workers_.take_front();
    worker->Join();
  }
}

void WorkerPool::Inner::PostTask(scoped_ptr<internal::WorkerPoolTask> task,
                                 bool signal_workers) {
  base::AutoLock lock(lock_);

  pending_tasks_.push_back(task.Pass());

  // There is more work available, so wake up worker thread.
  if (signal_workers)
    has_pending_tasks_cv_.Signal();
}

bool WorkerPool::Inner::CollectCompletedTasks() {
  base::AutoLock lock(lock_);

  return AppendCompletedTasksWithLockAcquired(
      &worker_pool_on_origin_thread_->completed_tasks_);
}

bool WorkerPool::Inner::RunCheapTasksUntilTimeLimit(
    base::TimeTicks time_limit) {
  base::AutoLock lock(lock_);

  while (base::TimeTicks::Now() < time_limit) {
    scoped_ptr<internal::WorkerPoolTask> task;

    // Find next cheap task.
    for (TaskDeque::iterator iter = pending_tasks_.begin();
         iter != pending_tasks_.end(); ++iter) {
      if ((*iter)->IsCheap()) {
        task = pending_tasks_.take(iter);
        break;
      }
    }

    if (!task) {
      // Schedule an idle callback if requested and not pending.
      if (!running_task_count_ && pending_tasks_.empty())
        ScheduleOnIdleWithLockAcquired();

      // Exit when no more cheap tasks are pending.
      break;
    }

    // Increment |running_task_count_| before starting to run task.
    running_task_count_++;

    {
      base::AutoUnlock unlock(lock_);

      task->Run();

      // Append tasks directly to worker pool's completed tasks queue.
      worker_pool_on_origin_thread_->completed_tasks_.push_back(task.Pass());
      if (need_on_task_completed_callback_)
        worker_pool_on_origin_thread_->OnTaskCompleted();
    }

    // Decrement |running_task_count_| now that we are done running task.
    running_task_count_--;
  }

  if (!pending_tasks_.empty())
    has_pending_tasks_cv_.Signal();

  // Append any other completed tasks before releasing lock.
  return AppendCompletedTasksWithLockAcquired(
      &worker_pool_on_origin_thread_->completed_tasks_);
}

bool WorkerPool::Inner::AppendCompletedTasksWithLockAcquired(
    ScopedPtrDeque<internal::WorkerPoolTask>* completed_tasks) {
  lock_.AssertAcquired();

  while (completed_tasks_.size())
    completed_tasks->push_back(completed_tasks_.take_front().Pass());

  return !running_task_count_ && pending_tasks_.empty();
}

void WorkerPool::Inner::ScheduleOnTaskCompletedWithLockAcquired() {
  lock_.AssertAcquired();

  if (on_task_completed_pending_ || !need_on_task_completed_callback_)
    return;
  origin_loop_->PostTask(FROM_HERE, on_task_completed_callback_);
  on_task_completed_pending_ = true;
}

void WorkerPool::Inner::OnTaskCompletedOnOriginThread() {
  {
    base::AutoLock lock(lock_);

    DCHECK(on_task_completed_pending_);
    on_task_completed_pending_ = false;

    AppendCompletedTasksWithLockAcquired(
        &worker_pool_on_origin_thread_->completed_tasks_);
  }

  worker_pool_on_origin_thread_->OnTaskCompleted();
}

void WorkerPool::Inner::ScheduleOnIdleWithLockAcquired() {
  lock_.AssertAcquired();

  if (on_idle_pending_)
    return;
  origin_loop_->PostTask(FROM_HERE, on_idle_callback_);
  on_idle_pending_ = true;
}

void WorkerPool::Inner::OnIdleOnOriginThread() {
  {
    base::AutoLock lock(lock_);

    DCHECK(on_idle_pending_);
    on_idle_pending_ = false;

    // Early out if no longer idle.
    if (running_task_count_ || !pending_tasks_.empty())
      return;

    AppendCompletedTasksWithLockAcquired(
        &worker_pool_on_origin_thread_->completed_tasks_);
  }

  worker_pool_on_origin_thread_->OnIdle();
}

void WorkerPool::Inner::Run() {
#if defined(OS_ANDROID)
  // TODO(epenner): Move thread priorities to base. (crbug.com/170549)
  int nice_value = 10; // Idle priority.
  setpriority(PRIO_PROCESS, base::PlatformThread::CurrentId(), nice_value);
#endif

  {
    base::AutoLock lock(lock_);

    // Get a unique thread index.
    int thread_index = next_thread_index_++;

    while (true) {
      if (pending_tasks_.empty()) {
        // Exit when shutdown is set and no more tasks are pending.
        if (shutdown_)
          break;

        // Schedule an idle callback if requested and not pending.
        if (!running_task_count_)
          ScheduleOnIdleWithLockAcquired();

        // Wait for new pending tasks.
        has_pending_tasks_cv_.Wait();
        continue;
      }

      // Get next task.
      scoped_ptr<internal::WorkerPoolTask> task = pending_tasks_.take_front();

      // Increment |running_task_count_| before starting to run task.
      running_task_count_++;

      // There may be more work available, so wake up another
      // worker thread.
      has_pending_tasks_cv_.Signal();

      {
        base::AutoUnlock unlock(lock_);

        task->RunOnThread(thread_index);
      }

      completed_tasks_.push_back(task.Pass());

      // Decrement |running_task_count_| now that we are done running task.
      running_task_count_--;

      // Schedule a task completed callback if requested and not pending.
      ScheduleOnTaskCompletedWithLockAcquired();
    }

    // We noticed we should exit. Wake up the next worker so it knows it should
    // exit as well (because the Shutdown() code only signals once).
    has_pending_tasks_cv_.Signal();
  }
}

WorkerPool::WorkerPool(WorkerPoolClient* client,
                       size_t num_threads,
                       base::TimeDelta check_for_completed_tasks_delay,
                       const std::string& thread_name_prefix)
    : client_(client),
      origin_loop_(base::MessageLoopProxy::current()),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      check_for_completed_tasks_delay_(check_for_completed_tasks_delay),
      check_for_completed_tasks_pending_(false),
      run_cheap_tasks_callback_(
          base::Bind(&WorkerPool::RunCheapTasks,
                     weak_ptr_factory_.GetWeakPtr())),
      run_cheap_tasks_pending_(false),
      inner_(make_scoped_ptr(
                 new Inner(
                     this,
                     num_threads,
                     thread_name_prefix,
                     // Request OnTaskCompleted() callback when check
                     // for completed tasks delay is 0.
                     check_for_completed_tasks_delay == base::TimeDelta()))) {
}

WorkerPool::~WorkerPool() {
  Shutdown();

  // Cancel all pending callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  DCHECK_EQ(completed_tasks_.size(), 0);
}

void WorkerPool::Shutdown() {
  inner_->Shutdown();
  DispatchCompletionCallbacks();
}

void WorkerPool::PostTaskAndReply(
    const Callback& task, const base::Closure& reply) {
  PostTask(make_scoped_ptr(new WorkerPoolTaskImpl(
                               task,
                               reply)).PassAs<internal::WorkerPoolTask>());
}

void WorkerPool::SetRunCheapTasksTimeLimit(
    base::TimeTicks run_cheap_tasks_time_limit) {
  run_cheap_tasks_time_limit_ = run_cheap_tasks_time_limit;
  ScheduleRunCheapTasks();
}

void WorkerPool::OnIdle() {
  TRACE_EVENT0("cc", "WorkerPool::OnIdle");

  DispatchCompletionCallbacks();
}

void WorkerPool::OnTaskCompleted() {
  TRACE_EVENT0("cc", "WorkerPool::OnTaskCompleted");

  DispatchCompletionCallbacks();
}

void WorkerPool::ScheduleCheckForCompletedTasks() {
  if (check_for_completed_tasks_pending_ ||
      check_for_completed_tasks_delay_ == base::TimeDelta())
    return;
  check_for_completed_tasks_callback_.Reset(
    base::Bind(&WorkerPool::CheckForCompletedTasks,
               weak_ptr_factory_.GetWeakPtr()));
  check_for_completed_tasks_time_ = base::TimeTicks::Now() +
      check_for_completed_tasks_delay_;
  origin_loop_->PostDelayedTask(
      FROM_HERE,
      check_for_completed_tasks_callback_.callback(),
      check_for_completed_tasks_delay_);
  check_for_completed_tasks_pending_ = true;
}

void WorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "WorkerPool::CheckForCompletedTasks");
  DCHECK(check_for_completed_tasks_pending_);
  check_for_completed_tasks_pending_ = false;

  // Schedule another check for completed tasks if not idle.
  if (!inner_->CollectCompletedTasks())
    ScheduleCheckForCompletedTasks();

  DispatchCompletionCallbacks();
}

void WorkerPool::CancelCheckForCompletedTasks() {
  if (!check_for_completed_tasks_pending_)
    return;

  check_for_completed_tasks_callback_.Cancel();
  check_for_completed_tasks_pending_ = false;
}

void WorkerPool::DispatchCompletionCallbacks() {
  TRACE_EVENT0("cc", "WorkerPool::DispatchCompletionCallbacks");

  if (completed_tasks_.empty())
    return;

  while (completed_tasks_.size()) {
    scoped_ptr<internal::WorkerPoolTask> task = completed_tasks_.take_front();
    task->DidComplete();
  }

  client_->DidFinishDispatchingWorkerPoolCompletionCallbacks();
}

void WorkerPool::PostTask(scoped_ptr<internal::WorkerPoolTask> task) {
  bool signal_workers = true;
  if (task->IsCheap()) {
    // To make cheap tasks more likely to run on the origin thread, don't wake
    // workers when posting them.
    signal_workers = false;
    ScheduleRunCheapTasks();
  }

  // Schedule check for completed tasks if not pending.
  ScheduleCheckForCompletedTasks();

  inner_->PostTask(task.Pass(), signal_workers);
}

void WorkerPool::ScheduleRunCheapTasks() {
  if (run_cheap_tasks_pending_)
    return;
  origin_loop_->PostTask(FROM_HERE, run_cheap_tasks_callback_);
  run_cheap_tasks_pending_ = true;
}

void WorkerPool::RunCheapTasks() {
  TRACE_EVENT0("cc", "WorkerPool::RunCheapTasks");
  DCHECK(run_cheap_tasks_pending_);
  run_cheap_tasks_pending_ = false;

  while (true) {
    base::TimeTicks time_limit = run_cheap_tasks_time_limit_;

    if (!check_for_completed_tasks_time_.is_null())
      time_limit = std::min(time_limit, check_for_completed_tasks_time_);

    bool is_idle = inner_->RunCheapTasksUntilTimeLimit(time_limit);

    base::TimeTicks now = base::TimeTicks::Now();
    if (now >= run_cheap_tasks_time_limit_) {
      TRACE_EVENT_INSTANT0("cc", "WorkerPool::RunCheapTasks out of time",
                           TRACE_EVENT_SCOPE_THREAD);
      break;
    }

    // We must be out of cheap tasks if this happens.
    if (check_for_completed_tasks_time_.is_null() ||
        now < check_for_completed_tasks_time_)
      break;

    TRACE_EVENT_INSTANT0("cc", "WorkerPool::RunCheapTasks check time",
                         TRACE_EVENT_SCOPE_THREAD);
    CancelCheckForCompletedTasks();
    DispatchCompletionCallbacks();
    // Schedule another check for completed tasks if not idle.
    if (!is_idle)
      ScheduleCheckForCompletedTasks();
  }
}

}  // namespace cc
