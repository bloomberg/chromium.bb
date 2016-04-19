// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/scheduler_thread_pool.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task_scheduler/utils.h"
#include "base/threading/thread_local.h"

namespace base {
namespace internal {

namespace {

// SchedulerThreadPool that owns the current thread, if any.
LazyInstance<ThreadLocalPointer<const SchedulerTaskExecutor>>::Leaky
    tls_current_thread_pool = LAZY_INSTANCE_INITIALIZER;

// A task runner that runs tasks with the PARALLEL ExecutionMode.
class SchedulerParallelTaskRunner : public TaskRunner {
 public:
  // Constructs a SchedulerParallelTaskRunner which can be used to post tasks so
  // long as |executor| is alive.
  // TODO(robliao): Find a concrete way to manage |executor|'s memory.
  SchedulerParallelTaskRunner(const TaskTraits& traits,
                              TaskTracker* task_tracker,
                              SchedulerTaskExecutor* executor)
      : traits_(traits), task_tracker_(task_tracker), executor_(executor) {}

  // TaskRunner:
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const Closure& closure,
                       TimeDelta delay) override {
    // Post the task as part of a one-off single-task Sequence.
    return PostTaskToExecutor(
        WrapUnique(
            new Task(from_here, closure, traits_,
                     delay.is_zero() ? TimeTicks() : TimeTicks::Now() + delay)),
        make_scoped_refptr(new Sequence), executor_, task_tracker_);
  }

  bool RunsTasksOnCurrentThread() const override {
    return tls_current_thread_pool.Get().Get() == executor_;
  }

 private:
  ~SchedulerParallelTaskRunner() override = default;

  const TaskTraits traits_;
  TaskTracker* const task_tracker_;
  SchedulerTaskExecutor* const executor_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerParallelTaskRunner);
};

// A task runner that runs tasks with the SEQUENCED ExecutionMode.
class SchedulerSequencedTaskRunner : public SequencedTaskRunner {
 public:
  // Constructs a SchedulerParallelTaskRunner which can be used to post tasks so
  // long as |executor| is alive.
  // TODO(robliao): Find a concrete way to manage |executor|'s memory.
  SchedulerSequencedTaskRunner(const TaskTraits& traits,
                               TaskTracker* task_tracker,
                               SchedulerTaskExecutor* executor)
      : traits_(traits), task_tracker_(task_tracker), executor_(executor) {}

  // SequencedTaskRunner:
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const Closure& closure,
                       TimeDelta delay) override {
    // Post the task as part of |sequence|.
    return PostTaskToExecutor(
        WrapUnique(
            new Task(from_here, closure, traits_,
                     delay.is_zero() ? TimeTicks() : TimeTicks::Now() + delay)),
        sequence_, executor_, task_tracker_);
  }

  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const Closure& closure,
                                  base::TimeDelta delay) override {
    // Tasks are never nested within the task scheduler.
    return PostDelayedTask(from_here, closure, delay);
  }

  bool RunsTasksOnCurrentThread() const override {
    return tls_current_thread_pool.Get().Get() == executor_;
  }

 private:
  ~SchedulerSequencedTaskRunner() override = default;

  // Sequence for all Tasks posted through this TaskRunner.
  const scoped_refptr<Sequence> sequence_ = new Sequence;

  const TaskTraits traits_;
  TaskTracker* const task_tracker_;
  SchedulerTaskExecutor* const executor_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerSequencedTaskRunner);
};

}  // namespace

class SchedulerThreadPool::SchedulerWorkerThreadDelegateImpl
    : public SchedulerWorkerThread::Delegate {
 public:
  SchedulerWorkerThreadDelegateImpl(
      SchedulerThreadPool* outer,
      const EnqueueSequenceCallback& enqueue_sequence_callback);
  ~SchedulerWorkerThreadDelegateImpl() override;

  // SchedulerWorkerThread::Delegate:
  void OnMainEntry() override;
  scoped_refptr<Sequence> GetWork(
      SchedulerWorkerThread* worker_thread) override;
  void EnqueueSequence(scoped_refptr<Sequence> sequence) override;

 private:
  SchedulerThreadPool* outer_;
  const EnqueueSequenceCallback enqueue_sequence_callback_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerWorkerThreadDelegateImpl);
};

SchedulerThreadPool::~SchedulerThreadPool() {
  // SchedulerThreadPool should never be deleted in production unless its
  // initialization failed.
  DCHECK(join_for_testing_returned_.IsSignaled() || worker_threads_.empty());
}

std::unique_ptr<SchedulerThreadPool> SchedulerThreadPool::CreateThreadPool(
    ThreadPriority thread_priority,
    size_t max_threads,
    const EnqueueSequenceCallback& enqueue_sequence_callback,
    TaskTracker* task_tracker) {
  std::unique_ptr<SchedulerThreadPool> thread_pool(
      new SchedulerThreadPool(enqueue_sequence_callback, task_tracker));
  if (thread_pool->Initialize(thread_priority, max_threads))
    return thread_pool;
  return nullptr;
}

scoped_refptr<TaskRunner> SchedulerThreadPool::CreateTaskRunnerWithTraits(
    const TaskTraits& traits,
    ExecutionMode execution_mode) {
  switch (execution_mode) {
    case ExecutionMode::PARALLEL:
      return make_scoped_refptr(
          new SchedulerParallelTaskRunner(traits, task_tracker_, this));

    case ExecutionMode::SEQUENCED:
      return make_scoped_refptr(
          new SchedulerSequencedTaskRunner(traits, task_tracker_, this));

    case ExecutionMode::SINGLE_THREADED:
      // TODO(fdoray): Support SINGLE_THREADED TaskRunners.
      NOTREACHED();
      return nullptr;
  }

  NOTREACHED();
  return nullptr;
}

void SchedulerThreadPool::EnqueueSequence(
    scoped_refptr<Sequence> sequence,
    const SequenceSortKey& sequence_sort_key) {
  shared_priority_queue_.BeginTransaction()->Push(
      WrapUnique(new PriorityQueue::SequenceAndSortKey(std::move(sequence),
                                                       sequence_sort_key)));

  // The thread calling this method just ran a Task from |sequence| and will
  // soon try to get another Sequence from which to run a Task. If the thread
  // belongs to this pool, it will get that Sequence from
  // |shared_priority_queue_|. When that's the case, there is no need to wake up
  // another thread after |sequence| is inserted in |shared_priority_queue_|. If
  // we did wake up another thread, we would waste resources by having more
  // threads trying to get a Sequence from |shared_priority_queue_| than the
  // number of Sequences in it.
  if (tls_current_thread_pool.Get().Get() != this)
    WakeUpOneThread();
}

void SchedulerThreadPool::WaitForAllWorkerThreadsIdleForTesting() {
  AutoSchedulerLock auto_lock(idle_worker_threads_stack_lock_);
  while (idle_worker_threads_stack_.size() < worker_threads_.size())
    idle_worker_threads_stack_cv_for_testing_->Wait();
}

void SchedulerThreadPool::JoinForTesting() {
  for (const auto& worker_thread : worker_threads_)
    worker_thread->JoinForTesting();

  DCHECK(!join_for_testing_returned_.IsSignaled());
  join_for_testing_returned_.Signal();
}

void SchedulerThreadPool::PostTaskWithSequence(
    std::unique_ptr<Task> task,
    scoped_refptr<Sequence> sequence) {
  DCHECK(task);
  DCHECK(sequence);

  const bool sequence_was_empty = AddTaskToSequenceAndPriorityQueue(
      std::move(task), std::move(sequence), &shared_priority_queue_);

  // No thread has already been woken up to run Tasks from |sequence| if it was
  // empty before |task| was inserted into it.
  if (sequence_was_empty)
    WakeUpOneThread();
}

SchedulerThreadPool::SchedulerWorkerThreadDelegateImpl::
    SchedulerWorkerThreadDelegateImpl(
        SchedulerThreadPool* outer,
        const EnqueueSequenceCallback& enqueue_sequence_callback)
    : outer_(outer), enqueue_sequence_callback_(enqueue_sequence_callback) {}

SchedulerThreadPool::SchedulerWorkerThreadDelegateImpl::
    ~SchedulerWorkerThreadDelegateImpl() = default;

void SchedulerThreadPool::SchedulerWorkerThreadDelegateImpl::OnMainEntry() {
  DCHECK(!tls_current_thread_pool.Get().Get());
  tls_current_thread_pool.Get().Set(outer_);
}

scoped_refptr<Sequence>
SchedulerThreadPool::SchedulerWorkerThreadDelegateImpl::GetWork(
    SchedulerWorkerThread* worker_thread) {
  std::unique_ptr<PriorityQueue::Transaction> transaction(
      outer_->shared_priority_queue_.BeginTransaction());
  const auto& sequence_and_sort_key = transaction->Peek();

  if (sequence_and_sort_key.is_null()) {
    // |transaction| is kept alive while |worker_thread| is added to
    // |idle_worker_threads_stack_| to avoid this race:
    // 1. This thread creates a Transaction, finds |shared_priority_queue_|
    //    empty and ends the Transaction.
    // 2. Other thread creates a Transaction, inserts a Sequence into
    //    |shared_priority_queue_| and ends the Transaction. This can't happen
    //    if the Transaction of step 1 is still active because because there can
    //    only be one active Transaction per PriorityQueue at a time.
    // 3. Other thread calls WakeUpOneThread(). No thread is woken up because
    //    |idle_worker_threads_stack_| is empty.
    // 4. This thread adds itself to |idle_worker_threads_stack_| and goes to
    //    sleep. No thread runs the Sequence inserted in step 2.
    outer_->AddToIdleWorkerThreadsStack(worker_thread);
    return nullptr;
  }

  scoped_refptr<Sequence> sequence = sequence_and_sort_key.sequence;
  transaction->Pop();
  return sequence;
}

void SchedulerThreadPool::SchedulerWorkerThreadDelegateImpl::EnqueueSequence(
    scoped_refptr<Sequence> sequence) {
  enqueue_sequence_callback_.Run(std::move(sequence));
}

SchedulerThreadPool::SchedulerThreadPool(
    const EnqueueSequenceCallback& enqueue_sequence_callback,
    TaskTracker* task_tracker)
    : idle_worker_threads_stack_lock_(shared_priority_queue_.container_lock()),
      idle_worker_threads_stack_cv_for_testing_(
          idle_worker_threads_stack_lock_.CreateConditionVariable()),
      join_for_testing_returned_(true, false),
      worker_thread_delegate_(
          new SchedulerWorkerThreadDelegateImpl(this,
                                                enqueue_sequence_callback)),
      task_tracker_(task_tracker) {
  DCHECK(task_tracker_);
}

bool SchedulerThreadPool::Initialize(ThreadPriority thread_priority,
                                     size_t max_threads) {
  AutoSchedulerLock auto_lock(idle_worker_threads_stack_lock_);

  DCHECK(worker_threads_.empty());

  for (size_t i = 0; i < max_threads; ++i) {
    std::unique_ptr<SchedulerWorkerThread> worker_thread =
        SchedulerWorkerThread::CreateSchedulerWorkerThread(
            thread_priority, worker_thread_delegate_.get(), task_tracker_);
    if (!worker_thread)
      break;
    idle_worker_threads_stack_.push(worker_thread.get());
    worker_threads_.push_back(std::move(worker_thread));
  }

  return !worker_threads_.empty();
}

void SchedulerThreadPool::WakeUpOneThread() {
  SchedulerWorkerThread* worker_thread = PopOneIdleWorkerThread();
  if (worker_thread)
    worker_thread->WakeUp();
}

void SchedulerThreadPool::AddToIdleWorkerThreadsStack(
    SchedulerWorkerThread* worker_thread) {
  AutoSchedulerLock auto_lock(idle_worker_threads_stack_lock_);
  idle_worker_threads_stack_.push(worker_thread);
  DCHECK_LE(idle_worker_threads_stack_.size(), worker_threads_.size());

  if (idle_worker_threads_stack_.size() == worker_threads_.size())
    idle_worker_threads_stack_cv_for_testing_->Broadcast();
}

SchedulerWorkerThread* SchedulerThreadPool::PopOneIdleWorkerThread() {
  AutoSchedulerLock auto_lock(idle_worker_threads_stack_lock_);

  if (idle_worker_threads_stack_.empty())
    return nullptr;

  auto worker_thread = idle_worker_threads_stack_.top();
  idle_worker_threads_stack_.pop();
  return worker_thread;
}

}  // namespace internal
}  // namespace base
