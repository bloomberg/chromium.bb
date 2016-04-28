// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/scheduler_thread_pool_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task_scheduler/delayed_task_manager.h"
#include "base/task_scheduler/task_tracker.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"

namespace base {
namespace internal {

namespace {

// SchedulerThreadPool that owns the current thread, if any.
LazyInstance<ThreadLocalPointer<const SchedulerThreadPool>>::Leaky
    tls_current_thread_pool = LAZY_INSTANCE_INITIALIZER;

// SchedulerWorkerThread that owns the current thread, if any.
LazyInstance<ThreadLocalPointer<const SchedulerWorkerThread>>::Leaky
    tls_current_worker_thread = LAZY_INSTANCE_INITIALIZER;

// A task runner that runs tasks with the PARALLEL ExecutionMode.
class SchedulerParallelTaskRunner : public TaskRunner {
 public:
  // Constructs a SchedulerParallelTaskRunner which can be used to post tasks so
  // long as |thread_pool| is alive.
  // TODO(robliao): Find a concrete way to manage |thread_pool|'s memory.
  SchedulerParallelTaskRunner(const TaskTraits& traits,
                              SchedulerThreadPool* thread_pool)
      : traits_(traits), thread_pool_(thread_pool) {}

  // TaskRunner:
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const Closure& closure,
                       TimeDelta delay) override {
    // Post the task as part of a one-off single-task Sequence.
    return thread_pool_->PostTaskWithSequence(
        WrapUnique(new Task(from_here, closure, traits_, delay)),
        make_scoped_refptr(new Sequence), nullptr);
  }

  bool RunsTasksOnCurrentThread() const override {
    return tls_current_thread_pool.Get().Get() == thread_pool_;
  }

 private:
  ~SchedulerParallelTaskRunner() override = default;

  const TaskTraits traits_;
  SchedulerThreadPool* const thread_pool_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerParallelTaskRunner);
};

// A task runner that runs tasks with the SEQUENCED ExecutionMode.
class SchedulerSequencedTaskRunner : public SequencedTaskRunner {
 public:
  // Constructs a SchedulerSequencedTaskRunner which can be used to post tasks
  // so long as |thread_pool| is alive.
  // TODO(robliao): Find a concrete way to manage |thread_pool|'s memory.
  SchedulerSequencedTaskRunner(const TaskTraits& traits,
                               SchedulerThreadPool* thread_pool)
      : traits_(traits), thread_pool_(thread_pool) {}

  // SequencedTaskRunner:
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const Closure& closure,
                       TimeDelta delay) override {
    // Post the task as part of |sequence_|.
    return thread_pool_->PostTaskWithSequence(
        WrapUnique(new Task(from_here, closure, traits_, delay)), sequence_,
        nullptr);
  }

  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const Closure& closure,
                                  base::TimeDelta delay) override {
    // Tasks are never nested within the task scheduler.
    return PostDelayedTask(from_here, closure, delay);
  }

  bool RunsTasksOnCurrentThread() const override {
    return tls_current_thread_pool.Get().Get() == thread_pool_;
  }

 private:
  ~SchedulerSequencedTaskRunner() override = default;

  // Sequence for all Tasks posted through this TaskRunner.
  const scoped_refptr<Sequence> sequence_ = new Sequence;

  const TaskTraits traits_;
  SchedulerThreadPool* const thread_pool_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerSequencedTaskRunner);
};

// A task runner that runs tasks with the SINGLE_THREADED ExecutionMode.
class SchedulerSingleThreadTaskRunner : public SingleThreadTaskRunner {
 public:
  // Constructs a SchedulerSingleThreadTaskRunner which can be used to post
  // tasks so long as |thread_pool| and |worker_thread| are alive.
  // TODO(robliao): Find a concrete way to manage the memory of |thread_pool|
  // and |worker_thread|.
  SchedulerSingleThreadTaskRunner(const TaskTraits& traits,
                                  SchedulerThreadPool* thread_pool,
                                  SchedulerWorkerThread* worker_thread)
      : traits_(traits),
        thread_pool_(thread_pool),
        worker_thread_(worker_thread) {}

  // SingleThreadTaskRunner:
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const Closure& closure,
                       TimeDelta delay) override {
    // Post the task to be executed by |worker_thread_| as part of |sequence_|.
    return thread_pool_->PostTaskWithSequence(
        WrapUnique(new Task(from_here, closure, traits_, delay)), sequence_,
        worker_thread_);
  }

  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const Closure& closure,
                                  base::TimeDelta delay) override {
    // Tasks are never nested within the task scheduler.
    return PostDelayedTask(from_here, closure, delay);
  }

  bool RunsTasksOnCurrentThread() const override {
    return tls_current_worker_thread.Get().Get() == worker_thread_;
  }

 private:
  ~SchedulerSingleThreadTaskRunner() override = default;

  // Sequence for all Tasks posted through this TaskRunner.
  const scoped_refptr<Sequence> sequence_ = new Sequence;

  const TaskTraits traits_;
  SchedulerThreadPool* const thread_pool_;
  SchedulerWorkerThread* const worker_thread_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerSingleThreadTaskRunner);
};

// Only used in DCHECKs.
bool ContainsWorkerThread(
    const std::vector<std::unique_ptr<SchedulerWorkerThread>>& worker_threads,
    const SchedulerWorkerThread* worker_thread) {
  auto it = std::find_if(
      worker_threads.begin(), worker_threads.end(),
      [worker_thread](const std::unique_ptr<SchedulerWorkerThread>& i) {
        return i.get() == worker_thread;
      });
  return it != worker_threads.end();
}

}  // namespace

class SchedulerThreadPoolImpl::SchedulerWorkerThreadDelegateImpl
    : public SchedulerWorkerThread::Delegate {
 public:
  // |outer| owns the worker thread for which this delegate is constructed.
  // |re_enqueue_sequence_callback| is invoked when ReEnqueueSequence() is
  // called with a non-single-threaded Sequence. |shared_priority_queue| is a
  // PriorityQueue whose transactions may overlap with the worker thread's
  // single-threaded PriorityQueue's transactions.
  SchedulerWorkerThreadDelegateImpl(
      SchedulerThreadPoolImpl* outer,
      const ReEnqueueSequenceCallback& re_enqueue_sequence_callback,
      const PriorityQueue* shared_priority_queue);
  ~SchedulerWorkerThreadDelegateImpl() override;

  PriorityQueue* single_threaded_priority_queue() {
    return &single_threaded_priority_queue_;
  }

  // SchedulerWorkerThread::Delegate:
  void OnMainEntry(SchedulerWorkerThread* worker_thread) override;
  scoped_refptr<Sequence> GetWork(
      SchedulerWorkerThread* worker_thread) override;
  void ReEnqueueSequence(scoped_refptr<Sequence> sequence) override;

 private:
  SchedulerThreadPoolImpl* outer_;
  const ReEnqueueSequenceCallback re_enqueue_sequence_callback_;

  // Single-threaded PriorityQueue for the worker thread.
  PriorityQueue single_threaded_priority_queue_;

  // True if the last Sequence returned by GetWork() was extracted from
  // |single_threaded_priority_queue_|.
  bool last_sequence_is_single_threaded_ = false;

  DISALLOW_COPY_AND_ASSIGN(SchedulerWorkerThreadDelegateImpl);
};

SchedulerThreadPoolImpl::~SchedulerThreadPoolImpl() {
  // SchedulerThreadPool should never be deleted in production unless its
  // initialization failed.
  DCHECK(join_for_testing_returned_.IsSignaled() || worker_threads_.empty());
}

// static
std::unique_ptr<SchedulerThreadPoolImpl> SchedulerThreadPoolImpl::Create(
    ThreadPriority thread_priority,
    size_t max_threads,
    IORestriction io_restriction,
    const ReEnqueueSequenceCallback& re_enqueue_sequence_callback,
    TaskTracker* task_tracker,
    DelayedTaskManager* delayed_task_manager) {
  std::unique_ptr<SchedulerThreadPoolImpl> thread_pool(
      new SchedulerThreadPoolImpl(io_restriction, task_tracker,
                                  delayed_task_manager));
  if (thread_pool->Initialize(thread_priority, max_threads,
                              re_enqueue_sequence_callback)) {
    return thread_pool;
  }
  return nullptr;
}

void SchedulerThreadPoolImpl::WaitForAllWorkerThreadsIdleForTesting() {
  AutoSchedulerLock auto_lock(idle_worker_threads_stack_lock_);
  while (idle_worker_threads_stack_.Size() < worker_threads_.size())
    idle_worker_threads_stack_cv_for_testing_->Wait();
}

void SchedulerThreadPoolImpl::JoinForTesting() {
  for (const auto& worker_thread : worker_threads_)
    worker_thread->JoinForTesting();

  DCHECK(!join_for_testing_returned_.IsSignaled());
  join_for_testing_returned_.Signal();
}

scoped_refptr<TaskRunner> SchedulerThreadPoolImpl::CreateTaskRunnerWithTraits(
    const TaskTraits& traits,
    ExecutionMode execution_mode) {
  switch (execution_mode) {
    case ExecutionMode::PARALLEL:
      return make_scoped_refptr(new SchedulerParallelTaskRunner(traits, this));

    case ExecutionMode::SEQUENCED:
      return make_scoped_refptr(new SchedulerSequencedTaskRunner(traits, this));

    case ExecutionMode::SINGLE_THREADED: {
      // TODO(fdoray): Find a way to take load into account when assigning a
      // SchedulerWorkerThread to a SingleThreadTaskRunner. Also, this code
      // assumes that all SchedulerWorkerThreads are alive. Eventually, we might
      // decide to tear down threads that haven't run tasks for a long time.
      size_t worker_thread_index;
      {
        AutoSchedulerLock auto_lock(next_worker_thread_index_lock_);
        worker_thread_index = next_worker_thread_index_;
        next_worker_thread_index_ =
            (next_worker_thread_index_ + 1) % worker_threads_.size();
      }
      return make_scoped_refptr(new SchedulerSingleThreadTaskRunner(
          traits, this, worker_threads_[worker_thread_index].get()));
    }
  }

  NOTREACHED();
  return nullptr;
}

void SchedulerThreadPoolImpl::ReEnqueueSequence(
    scoped_refptr<Sequence> sequence,
    const SequenceSortKey& sequence_sort_key) {
  shared_priority_queue_.BeginTransaction()->Push(std::move(sequence),
                                                  sequence_sort_key);

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

bool SchedulerThreadPoolImpl::PostTaskWithSequence(
    std::unique_ptr<Task> task,
    scoped_refptr<Sequence> sequence,
    SchedulerWorkerThread* worker_thread) {
  DCHECK(task);
  DCHECK(sequence);
  DCHECK(!worker_thread ||
         ContainsWorkerThread(worker_threads_, worker_thread));

  if (!task_tracker_->WillPostTask(task.get()))
    return false;

  if (task->delayed_run_time.is_null()) {
    PostTaskWithSequenceNow(std::move(task), std::move(sequence),
                            worker_thread);
  } else {
    delayed_task_manager_->AddDelayedTask(std::move(task), std::move(sequence),
                                          worker_thread, this);
  }

  return true;
}

void SchedulerThreadPoolImpl::PostTaskWithSequenceNow(
    std::unique_ptr<Task> task,
    scoped_refptr<Sequence> sequence,
    SchedulerWorkerThread* worker_thread) {
  DCHECK(task);
  DCHECK(sequence);
  DCHECK(!worker_thread ||
         ContainsWorkerThread(worker_threads_, worker_thread));

  // Confirm that |task| is ready to run (its delayed run time is either null or
  // in the past).
  DCHECK_LE(task->delayed_run_time, delayed_task_manager_->Now());

  // Because |worker_thread| belongs to this thread pool, we know that the type
  // of its delegate is SchedulerWorkerThreadDelegateImpl.
  PriorityQueue* const priority_queue =
      worker_thread
          ? static_cast<SchedulerWorkerThreadDelegateImpl*>(
                worker_thread->delegate())
                ->single_threaded_priority_queue()
          : &shared_priority_queue_;
  DCHECK(priority_queue);

  const bool sequence_was_empty = sequence->PushTask(std::move(task));
  if (sequence_was_empty) {
    // Insert |sequence| in |priority_queue| if it was empty before |task| was
    // inserted into it. Otherwise, one of these must be true:
    // - |sequence| is already in a PriorityQueue (not necessarily
    //   |shared_priority_queue_|), or,
    // - A worker thread is running a Task from |sequence|. It will insert
    //   |sequence| in a PriorityQueue once it's done running the Task.
    const auto sequence_sort_key = sequence->GetSortKey();
    priority_queue->BeginTransaction()->Push(std::move(sequence),
                                             sequence_sort_key);

    // Wake up a worker thread to process |sequence|.
    if (worker_thread)
      worker_thread->WakeUp();
    else
      WakeUpOneThread();
  }
}

SchedulerThreadPoolImpl::SchedulerWorkerThreadDelegateImpl::
    SchedulerWorkerThreadDelegateImpl(
        SchedulerThreadPoolImpl* outer,
        const ReEnqueueSequenceCallback& re_enqueue_sequence_callback,
        const PriorityQueue* shared_priority_queue)
    : outer_(outer),
      re_enqueue_sequence_callback_(re_enqueue_sequence_callback),
      single_threaded_priority_queue_(shared_priority_queue) {}

SchedulerThreadPoolImpl::SchedulerWorkerThreadDelegateImpl::
    ~SchedulerWorkerThreadDelegateImpl() = default;

void SchedulerThreadPoolImpl::SchedulerWorkerThreadDelegateImpl::OnMainEntry(
    SchedulerWorkerThread* worker_thread) {
#if DCHECK_IS_ON()
  // Wait for |outer_->threads_created_| to avoid traversing
  // |outer_->worker_threads_| while it is being filled by Initialize().
  outer_->threads_created_.Wait();
  DCHECK(ContainsWorkerThread(outer_->worker_threads_, worker_thread));
#endif

  DCHECK(!tls_current_worker_thread.Get().Get());
  DCHECK(!tls_current_thread_pool.Get().Get());
  tls_current_worker_thread.Get().Set(worker_thread);
  tls_current_thread_pool.Get().Set(outer_);

  ThreadRestrictions::SetIOAllowed(outer_->io_restriction_ ==
                                   IORestriction::ALLOWED);
}

scoped_refptr<Sequence>
SchedulerThreadPoolImpl::SchedulerWorkerThreadDelegateImpl::GetWork(
    SchedulerWorkerThread* worker_thread) {
  DCHECK(ContainsWorkerThread(outer_->worker_threads_, worker_thread));

  scoped_refptr<Sequence> sequence;
  {
    std::unique_ptr<PriorityQueue::Transaction> shared_transaction(
        outer_->shared_priority_queue_.BeginTransaction());
    std::unique_ptr<PriorityQueue::Transaction> single_threaded_transaction(
        single_threaded_priority_queue_.BeginTransaction());

    if (shared_transaction->IsEmpty() &&
        single_threaded_transaction->IsEmpty()) {
      single_threaded_transaction.reset();

      // |shared_transaction| is kept alive while |worker_thread| is added to
      // |idle_worker_threads_stack_| to avoid this race:
      // 1. This thread creates a Transaction, finds |shared_priority_queue_|
      //    empty and ends the Transaction.
      // 2. Other thread creates a Transaction, inserts a Sequence into
      //    |shared_priority_queue_| and ends the Transaction. This can't happen
      //    if the Transaction of step 1 is still active because because there
      //    can only be one active Transaction per PriorityQueue at a time.
      // 3. Other thread calls WakeUpOneThread(). No thread is woken up because
      //    |idle_worker_threads_stack_| is empty.
      // 4. This thread adds itself to |idle_worker_threads_stack_| and goes to
      //    sleep. No thread runs the Sequence inserted in step 2.
      outer_->AddToIdleWorkerThreadsStack(worker_thread);
      return nullptr;
    }

    // True if both PriorityQueues have Sequences and the Sequence at the top of
    // the shared PriorityQueue is more important.
    const bool shared_sequence_is_more_important =
        !shared_transaction->IsEmpty() &&
        !single_threaded_transaction->IsEmpty() &&
        shared_transaction->PeekSortKey() >
            single_threaded_transaction->PeekSortKey();

    if (single_threaded_transaction->IsEmpty() ||
        shared_sequence_is_more_important) {
      sequence = shared_transaction->PopSequence();
      last_sequence_is_single_threaded_ = false;
    } else {
      DCHECK(!single_threaded_transaction->IsEmpty());
      sequence = single_threaded_transaction->PopSequence();
      last_sequence_is_single_threaded_ = true;
    }
  }
  DCHECK(sequence);

  outer_->RemoveFromIdleWorkerThreadsStack(worker_thread);
  return sequence;
}

void SchedulerThreadPoolImpl::SchedulerWorkerThreadDelegateImpl::
    ReEnqueueSequence(scoped_refptr<Sequence> sequence) {
  if (last_sequence_is_single_threaded_) {
    // A single-threaded Sequence is always re-enqueued in the single-threaded
    // PriorityQueue from which it was extracted.
    const SequenceSortKey sequence_sort_key = sequence->GetSortKey();
    single_threaded_priority_queue_.BeginTransaction()->Push(
        std::move(sequence), sequence_sort_key);
  } else {
    // |re_enqueue_sequence_callback_| will determine in which PriorityQueue
    // |sequence| must be enqueued.
    re_enqueue_sequence_callback_.Run(std::move(sequence));
  }
}

SchedulerThreadPoolImpl::SchedulerThreadPoolImpl(
    IORestriction io_restriction,
    TaskTracker* task_tracker,
    DelayedTaskManager* delayed_task_manager)
    : io_restriction_(io_restriction),
      idle_worker_threads_stack_lock_(shared_priority_queue_.container_lock()),
      idle_worker_threads_stack_cv_for_testing_(
          idle_worker_threads_stack_lock_.CreateConditionVariable()),
      join_for_testing_returned_(true, false),
#if DCHECK_IS_ON()
      threads_created_(true, false),
#endif
      task_tracker_(task_tracker),
      delayed_task_manager_(delayed_task_manager) {
  DCHECK(task_tracker_);
  DCHECK(delayed_task_manager_);
}

bool SchedulerThreadPoolImpl::Initialize(
    ThreadPriority thread_priority,
    size_t max_threads,
    const ReEnqueueSequenceCallback& re_enqueue_sequence_callback) {
  AutoSchedulerLock auto_lock(idle_worker_threads_stack_lock_);

  DCHECK(worker_threads_.empty());

  for (size_t i = 0; i < max_threads; ++i) {
    std::unique_ptr<SchedulerWorkerThread> worker_thread =
        SchedulerWorkerThread::Create(
            thread_priority,
            WrapUnique(new SchedulerWorkerThreadDelegateImpl(
                this, re_enqueue_sequence_callback, &shared_priority_queue_)),
            task_tracker_);
    if (!worker_thread)
      break;
    idle_worker_threads_stack_.Push(worker_thread.get());
    worker_threads_.push_back(std::move(worker_thread));
  }

#if DCHECK_IS_ON()
  threads_created_.Signal();
#endif

  return !worker_threads_.empty();
}

void SchedulerThreadPoolImpl::WakeUpOneThread() {
  SchedulerWorkerThread* worker_thread;
  {
    AutoSchedulerLock auto_lock(idle_worker_threads_stack_lock_);
    worker_thread = idle_worker_threads_stack_.Pop();
  }
  if (worker_thread)
    worker_thread->WakeUp();
}

void SchedulerThreadPoolImpl::AddToIdleWorkerThreadsStack(
    SchedulerWorkerThread* worker_thread) {
  AutoSchedulerLock auto_lock(idle_worker_threads_stack_lock_);
  idle_worker_threads_stack_.Push(worker_thread);
  DCHECK_LE(idle_worker_threads_stack_.Size(), worker_threads_.size());

  if (idle_worker_threads_stack_.Size() == worker_threads_.size())
    idle_worker_threads_stack_cv_for_testing_->Broadcast();
}

void SchedulerThreadPoolImpl::RemoveFromIdleWorkerThreadsStack(
    SchedulerWorkerThread* worker_thread) {
  AutoSchedulerLock auto_lock(idle_worker_threads_stack_lock_);
  idle_worker_threads_stack_.Remove(worker_thread);
}

}  // namespace internal
}  // namespace base
