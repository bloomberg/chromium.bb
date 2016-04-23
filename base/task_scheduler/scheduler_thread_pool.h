// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_SCHEDULER_THREAD_POOL_H_
#define BASE_TASK_SCHEDULER_SCHEDULER_THREAD_POOL_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/condition_variable.h"
#include "base/task_runner.h"
#include "base/task_scheduler/priority_queue.h"
#include "base/task_scheduler/scheduler_lock.h"
#include "base/task_scheduler/scheduler_task_executor.h"
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
class BASE_EXPORT SchedulerThreadPool : public SchedulerTaskExecutor {
 public:
  // Callback invoked when a Sequence isn't empty after a worker thread pops a
  // Task from it.
  using EnqueueSequenceCallback = Callback<void(scoped_refptr<Sequence>)>;

  // Destroying a SchedulerThreadPool returned by CreateThreadPool() is not
  // allowed in production; it is always leaked. In tests, it can only be
  // destroyed after JoinForTesting() has returned.
  ~SchedulerThreadPool() override;

  // Creates a SchedulerThreadPool with up to |max_threads| threads of priority
  // |thread_priority|. |enqueue_sequence_callback| will be invoked after a
  // thread of this thread pool tries to run a Task. |task_tracker| is used to
  // handle shutdown behavior of Tasks. |delayed_task_manager| handles Tasks
  // posted with a delay. Returns nullptr on failure to create a thread pool
  // with at least one thread.
  static std::unique_ptr<SchedulerThreadPool> CreateThreadPool(
      ThreadPriority thread_priority,
      size_t max_threads,
      const EnqueueSequenceCallback& enqueue_sequence_callback,
      TaskTracker* task_tracker,
      DelayedTaskManager* delayed_task_manager);

  // Returns a TaskRunner whose PostTask invocations will result in scheduling
  // Tasks with |traits| and |execution_mode| in this thread pool.
  scoped_refptr<TaskRunner> CreateTaskRunnerWithTraits(
      const TaskTraits& traits,
      ExecutionMode execution_mode);

  // Inserts |sequence| into this thread pool's shared priority queue with
  // |sequence_sort_key|. Must only be called from a worker thread to put
  // |sequence| back into a PriorityQueue after running a Task from it. The
  // worker thread doesn't have to belong to this thread pool.
  void EnqueueSequence(scoped_refptr<Sequence> sequence,
                       const SequenceSortKey& sequence_sort_key);

  // Waits until all threads are idle.
  void WaitForAllWorkerThreadsIdleForTesting();

  // Joins all threads of this thread pool. Tasks that are already running are
  // allowed to complete their execution. This can only be called once.
  void JoinForTesting();

  // SchedulerTaskExecutor:
  void PostTaskWithSequence(std::unique_ptr<Task> task,
                            scoped_refptr<Sequence> sequence) override;

 private:
  class SchedulerWorkerThreadDelegateImpl;

  SchedulerThreadPool(const EnqueueSequenceCallback& enqueue_sequence_callback,
                      TaskTracker* task_tracker,
                      DelayedTaskManager* delayed_task_manager);

  bool Initialize(ThreadPriority thread_priority, size_t max_threads);

  // Wakes up the last thread from this thread pool to go idle, if any.
  void WakeUpOneThread();

  // Adds |worker_thread| to |idle_worker_threads_stack_|.
  void AddToIdleWorkerThreadsStack(SchedulerWorkerThread* worker_thread);

  // PriorityQueue from which all threads of this thread pool get work.
  PriorityQueue shared_priority_queue_;

  // All worker threads owned by this thread pool. Only modified during
  // initialization of the thread pool.
  std::vector<std::unique_ptr<SchedulerWorkerThread>> worker_threads_;

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

  // Delegate for all worker threads in this pool.
  std::unique_ptr<SchedulerWorkerThread::Delegate> worker_thread_delegate_;

  TaskTracker* const task_tracker_;
  DelayedTaskManager* const delayed_task_manager_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerThreadPool);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_SCHEDULER_SCHEDULER_THREAD_POOL_H_
