// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_IMPL_ATTACHMENTS_TASK_QUEUE_H_
#define COMPONENTS_SYNC_MODEL_IMPL_ATTACHMENTS_TASK_QUEUE_H_

#include <stddef.h>

#include <deque>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"

namespace syncer {

// A queue that dispatches tasks, ignores duplicates, and provides backoff
// semantics.
//
// |T| is the task type.
//
// For each task added to the queue, the HandleTaskCallback will eventually be
// invoked.  For each invocation, the user of TaskQueue must call exactly one of
// |MarkAsSucceeded|, |MarkAsFailed|, or |Cancel|.
//
// To retry a failed task, call MarkAsFailed(task) then AddToQueue(task).
//
// Example usage:
//
// void Handle(const Foo& foo);
// ...
// TaskQueue<Foo> queue(base::Bind(&Handle),
//                      base::TimeDelta::FromSeconds(1),
//                      base::TimeDelta::FromMinutes(1));
// ...
// {
//   Foo foo;
//   // Add foo to the queue.  At some point, Handle will be invoked in this
//   // message loop.
//   queue.AddToQueue(foo);
// }
// ...
// void Handle(const Foo& foo) {
//   DoSomethingWith(foo);
//   // We must call one of the three methods to tell the queue how we're
//   // dealing with foo.  Of course, we are free to call in the the context of
//   // this HandleTaskCallback or outside the context if we so choose.
//   if (SuccessfullyHandled(foo)) {
//     queue.MarkAsSucceeded(foo);
//   } else if (Failed(foo)) {
//     queue.MarkAsFailed(foo);
//     if (ShouldRetry(foo)) {
//       queue.AddToQueue(foo);
//     }
//   } else {
//     Cancel(foo);
//   }
// }
//
template <typename T>
class TaskQueue {
 public:
  // A callback provided by users of the TaskQueue to handle tasks.
  //
  // This callback is invoked by the queue with a task to be handled.  The
  // callee is expected to (eventually) call |MarkAsSucceeded|, |MarkAsFailed|,
  // or |Cancel| to signify completion of the task.
  using HandleTaskCallback = base::Callback<void(const T&)>;

  // Construct a TaskQueue.
  //
  // |callback| the callback to be invoked for handling tasks.
  //
  // |initial_backoff_delay| the initial amount of time the queue will wait
  // before dispatching tasks after a failed task (see |MarkAsFailed|).  May be
  // zero.  Subsequent failures will increase the delay up to
  // |max_backoff_delay|.
  //
  // |max_backoff_delay| the maximum amount of time the queue will wait before
  // dispatching tasks.  May be zero.  Must be greater than or equal to
  // |initial_backoff_delay|.
  TaskQueue(const HandleTaskCallback& callback,
            const base::TimeDelta& initial_backoff_delay,
            const base::TimeDelta& max_backoff_delay);

  ~TaskQueue();

  // Add |task| to the end of the queue.
  //
  // If |task| is already present (as determined by operator==) it is not added.
  void AddToQueue(const T& task);

  // Mark |task| as completing successfully.
  //
  // Marking a task as completing successfully will reduce or eliminate any
  // backoff delay in effect.
  //
  // May only be called after the HandleTaskCallback has been invoked with
  // |task|.
  void MarkAsSucceeded(const T& task);

  // Mark |task| as failed.
  //
  // Marking a task as failed will cause a backoff, i.e. a delay in dispatching
  // of subsequent tasks.  Repeated failures will increase the delay.
  //
  // May only be called after the HandleTaskCallback has been invoked with
  // |task|.
  void MarkAsFailed(const T& task);

  // Cancel |task|.
  //
  // |task| is removed from the queue and will not be retried.  Does not affect
  // the backoff delay.
  //
  // May only be called after the HandleTaskCallback has been invoked with
  // |task|.
  void Cancel(const T& task);

  // Reset any backoff delay and resume dispatching of tasks.
  //
  // Useful for when you know the cause of previous failures has been resolved
  // and you want don't want to wait for the accumulated backoff delay to
  // elapse.
  void ResetBackoff();

  // Use |timer| for scheduled events.
  //
  // Used in tests.  See also MockTimer.
  void SetTimerForTest(std::unique_ptr<base::Timer> timer);

 private:
  void FinishTask(const T& task);
  void ScheduleDispatch();
  void Dispatch();
  // Return true if we should dispatch tasks.
  bool ShouldDispatch();

  const HandleTaskCallback process_callback_;
  net::BackoffEntry::Policy backoff_policy_;
  std::unique_ptr<net::BackoffEntry> backoff_entry_;
  // The number of tasks currently being handled.
  int num_in_progress_;
  std::deque<T> queue_;
  // The set of tasks in queue_ or currently being handled.
  std::set<T> tasks_;
  base::Closure dispatch_closure_;
  std::unique_ptr<base::Timer> backoff_timer_;
  base::TimeDelta delay_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Must be last data member.
  base::WeakPtrFactory<TaskQueue> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueue);
};

// The maximum number of tasks that may be concurrently executed.  Think
// carefully before changing this value.  The desired behavior of backoff may
// not be obvious when there is more than one concurrent task
const int kMaxConcurrentTasks = 1;

template <typename T>
TaskQueue<T>::TaskQueue(const HandleTaskCallback& callback,
                        const base::TimeDelta& initial_backoff_delay,
                        const base::TimeDelta& max_backoff_delay)
    : process_callback_(callback),
      backoff_policy_({}),
      num_in_progress_(0),
      weak_ptr_factory_(this) {
  DCHECK_LE(initial_backoff_delay.InMicroseconds(),
            max_backoff_delay.InMicroseconds());
  backoff_policy_.initial_delay_ms = initial_backoff_delay.InMilliseconds();
  backoff_policy_.multiply_factor = 2.0;
  backoff_policy_.jitter_factor = 0.1;
  backoff_policy_.maximum_backoff_ms = max_backoff_delay.InMilliseconds();
  backoff_policy_.entry_lifetime_ms = -1;
  backoff_policy_.always_use_initial_delay = false;
  backoff_entry_ = base::MakeUnique<net::BackoffEntry>(&backoff_policy_);
  dispatch_closure_ =
      base::Bind(&TaskQueue::Dispatch, weak_ptr_factory_.GetWeakPtr());
  backoff_timer_ = base::MakeUnique<base::Timer>(false, false);
}

template <typename T>
TaskQueue<T>::~TaskQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

template <typename T>
void TaskQueue<T>::AddToQueue(const T& task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ignore duplicates.
  if (tasks_.find(task) == tasks_.end()) {
    queue_.push_back(task);
    tasks_.insert(task);
  }
  ScheduleDispatch();
}

template <typename T>
void TaskQueue<T>::MarkAsSucceeded(const T& task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FinishTask(task);
  // The task succeeded.  Stop any pending timer, reset (clear) the backoff, and
  // reschedule a dispatch.
  backoff_timer_->Stop();
  backoff_entry_->Reset();
  ScheduleDispatch();
}

template <typename T>
void TaskQueue<T>::MarkAsFailed(const T& task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FinishTask(task);
  backoff_entry_->InformOfRequest(false);
  ScheduleDispatch();
}

template <typename T>
void TaskQueue<T>::Cancel(const T& task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FinishTask(task);
  ScheduleDispatch();
}

template <typename T>
void TaskQueue<T>::ResetBackoff() {
  backoff_timer_->Stop();
  backoff_entry_->Reset();
  ScheduleDispatch();
}

template <typename T>
void TaskQueue<T>::SetTimerForTest(std::unique_ptr<base::Timer> timer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(timer.get());
  backoff_timer_ = std::move(timer);
}

template <typename T>
void TaskQueue<T>::FinishTask(const T& task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(num_in_progress_, 1);
  --num_in_progress_;
  const size_t num_erased = tasks_.erase(task);
  DCHECK_EQ(1U, num_erased);
}

template <typename T>
void TaskQueue<T>::ScheduleDispatch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (backoff_timer_->IsRunning() || !ShouldDispatch()) {
    return;
  }

  backoff_timer_->Start(FROM_HERE, backoff_entry_->GetTimeUntilRelease(),
                        dispatch_closure_);
}

template <typename T>
void TaskQueue<T>::Dispatch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ShouldDispatch()) {
    return;
  }

  DCHECK(!queue_.empty());
  const T& task = queue_.front();
  ++num_in_progress_;
  DCHECK_LE(num_in_progress_, kMaxConcurrentTasks);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(process_callback_, task));
  queue_.pop_front();
}

template <typename T>
bool TaskQueue<T>::ShouldDispatch() {
  return num_in_progress_ < kMaxConcurrentTasks && !queue_.empty();
}

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_IMPL_ATTACHMENTS_TASK_QUEUE_H_
