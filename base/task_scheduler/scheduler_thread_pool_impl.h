// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_SCHEDULER_THREAD_POOL_IMPL_H_
#define BASE_TASK_SCHEDULER_SCHEDULER_THREAD_POOL_IMPL_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/condition_variable.h"
#include "base/task_runner.h"
#include "base/task_scheduler/priority_queue.h"
#include "base/task_scheduler/scheduler_lock.h"
#include "base/task_scheduler/scheduler_thread_pool.h"
#include "base/task_scheduler/scheduler_worker_thread.h"
#include "base/task_scheduler/scheduler_worker_thread_stack.h"
#include "base/task_scheduler/sequence.h"
#include "base/task_scheduler/task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/platform_thread.h"

namespace base {
namespace internal {

class DelayedTaskManager;
struct SequenceSortKey;
class TaskTracker;

// A pool of threads that run Tasks. This class is thread-safe.
class BASE_EXPORT SchedulerThreadPoolImpl : public SchedulerThreadPool {
 public:
  // Callback invoked when a Sequence isn't empty after a worker thread pops a
  // Task from it.
  using ReEnqueueSequenceCallback = Callback<void(scoped_refptr<Sequence>)>;

  // Destroying a SchedulerThreadPool returned by CreateThreadPool() is not
  // allowed in production; it is always leaked. In tests, it can only be
  // destroyed after JoinForTesting() has returned.
  ~SchedulerThreadPoolImpl() override;

  // Creates a SchedulerThreadPool with up to |max_threads| threads of priority
  // |thread_priority|. |re_enqueue_sequence_callback| will be invoked after a
  // thread of this thread pool tries to run a Task. |task_tracker| is used to
  // handle shutdown behavior of Tasks. |delayed_task_manager| handles Tasks
  // posted with a delay. Returns nullptr on failure to create a thread pool
  // with at least one thread.
  static std::unique_ptr<SchedulerThreadPoolImpl> Create(
      ThreadPriority thread_priority,
      size_t max_threads,
      const ReEnqueueSequenceCallback& re_enqueue_sequence_callback,
      TaskTracker* task_tracker,
      DelayedTaskManager* delayed_task_manager);

  // Waits until all threads are idle.
  void WaitForAllWorkerThreadsIdleForTesting();

  // Joins all threads of this thread pool. Tasks that are already running are
  // allowed to complete their execution. This can only be called once.
  void JoinForTesting();

  // SchedulerThreadPool:
  scoped_refptr<TaskRunner> CreateTaskRunnerWithTraits(
      const TaskTraits& traits,
      ExecutionMode execution_mode) override;
  void ReEnqueueSequence(scoped_refptr<Sequence> sequence,
                         const SequenceSortKey& sequence_sort_key) override;
  bool PostTaskWithSequence(std::unique_ptr<Task> task,
                            scoped_refptr<Sequence> sequence,
                            SchedulerWorkerThread* worker_thread) override;
  void PostTaskWithSequenceNow(std::unique_ptr<Task> task,
                               scoped_refptr<Sequence> sequence,
                               SchedulerWorkerThread* worker_thread) override;

 private:
  class SchedulerWorkerThreadDelegateImpl;

  SchedulerThreadPoolImpl(TaskTracker* task_tracker,
                          DelayedTaskManager* delayed_task_manager);

  bool Initialize(
      ThreadPriority thread_priority,
      size_t max_threads,
      const ReEnqueueSequenceCallback& re_enqueue_sequence_callback);

  // Wakes up the last thread from this thread pool to go idle, if any.
  void WakeUpOneThread();

  // Adds |worker_thread| to |idle_worker_threads_stack_|.
  void AddToIdleWorkerThreadsStack(SchedulerWorkerThread* worker_thread);

  // Removes |worker_thread| from |idle_worker_threads_stack_|.
  void RemoveFromIdleWorkerThreadsStack(SchedulerWorkerThread* worker_thread);

  // All worker threads owned by this thread pool. Only modified during
  // initialization of the thread pool.
  std::vector<std::unique_ptr<SchedulerWorkerThread>> worker_threads_;

  // Synchronizes access to |next_worker_thread_index_|.
  SchedulerLock next_worker_thread_index_lock_;

  // Index of the worker thread that will be assigned to the next single-
  // threaded TaskRunner returned by this pool.
  size_t next_worker_thread_index_ = 0;

  // PriorityQueue from which all threads of this thread pool get work.
  PriorityQueue shared_priority_queue_;

  // Synchronizes access to |idle_worker_threads_stack_| and
  // |idle_worker_threads_stack_cv_for_testing_|. Has |shared_priority_queue_|'s
  // lock as its predecessor so that a thread can be pushed to
  // |idle_worker_threads_stack_| within the scope of a Transaction (more
  // details in GetWork()).
  SchedulerLock idle_worker_threads_stack_lock_;

  // Stack of idle worker threads.
  SchedulerWorkerThreadStack idle_worker_threads_stack_;

  // Signaled when all worker threads become idle.
  std::unique_ptr<ConditionVariable> idle_worker_threads_stack_cv_for_testing_;

  // Signaled once JoinForTesting() has returned.
  WaitableEvent join_for_testing_returned_;

#if DCHECK_IS_ON()
  // Signaled when all threads have been created.
  WaitableEvent threads_created_;
#endif

  TaskTracker* const task_tracker_;
  DelayedTaskManager* const delayed_task_manager_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerThreadPoolImpl);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_SCHEDULER_SCHEDULER_THREAD_POOL_IMPL_H_
