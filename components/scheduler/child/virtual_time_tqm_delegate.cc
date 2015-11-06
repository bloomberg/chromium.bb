// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/virtual_time_tqm_delegate.h"

namespace scheduler {

// static
scoped_refptr<VirtualTimeTqmDelegate> VirtualTimeTqmDelegate::Create(
    base::MessageLoop* message_loop,
    base::TimeTicks initial_now) {
  return make_scoped_refptr(
      new VirtualTimeTqmDelegate(message_loop, initial_now));
}

VirtualTimeTqmDelegate::VirtualTimeTqmDelegate(base::MessageLoop* message_loop,
                                               base::TimeTicks initial_now)
    : message_loop_(message_loop),
      message_loop_task_runner_(message_loop->task_runner()),
      now_(initial_now) {}

VirtualTimeTqmDelegate::~VirtualTimeTqmDelegate() {
  RestoreDefaultTaskRunner();
}

void VirtualTimeTqmDelegate::SetDefaultTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  message_loop_->SetTaskRunner(task_runner);
}

void VirtualTimeTqmDelegate::RestoreDefaultTaskRunner() {
  if (base::MessageLoop::current() == message_loop_)
    message_loop_->SetTaskRunner(message_loop_task_runner_);
}

bool VirtualTimeTqmDelegate::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  if (delay > base::TimeDelta()) {
    base::TimeTicks fire_time = now_ + delay;
    delayed_wakeup_multimap_.insert(std::make_pair(fire_time, task));
  }
  return message_loop_task_runner_->PostDelayedTask(from_here, task, delay);
}

bool VirtualTimeTqmDelegate::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return message_loop_task_runner_->PostNonNestableDelayedTask(from_here, task,
                                                               delay);
}

bool VirtualTimeTqmDelegate::RunsTasksOnCurrentThread() const {
  return message_loop_task_runner_->RunsTasksOnCurrentThread();
}

bool VirtualTimeTqmDelegate::IsNested() const {
  return message_loop_->IsNested();
}

base::TimeTicks VirtualTimeTqmDelegate::NowTicks() {
  return now_;
}

double VirtualTimeTqmDelegate::CurrentTimeSeconds() const {
  return (now_ - base::TimeTicks::UnixEpoch()).InSecondsF();
}

void VirtualTimeTqmDelegate::AdvancedTimeTo(base::TimeTicks now) {
  now_ = now;
  DCHECK_GE(now, now_);
  while (!delayed_wakeup_multimap_.empty()) {
    DelayedWakeupMultimap::iterator next_wakeup =
        delayed_wakeup_multimap_.begin();
    if (next_wakeup->first > now)
      break;
    message_loop_task_runner_->PostDelayedTask(FROM_HERE, next_wakeup->second,
                                               base::TimeDelta());
    delayed_wakeup_multimap_.erase(next_wakeup);
  }
}

void VirtualTimeTqmDelegate::OnNoMoreImmediateWork() {
  if (delayed_wakeup_multimap_.empty())
    return;

  AdvancedTimeTo(delayed_wakeup_multimap_.begin()->first);
}

}  // namespace scheduler
