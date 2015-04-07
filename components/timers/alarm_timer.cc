// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/timers/alarm_timer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/pending_task.h"
#include "components/timers/rtc_alarm.h"

namespace timers {

AlarmTimer::AlarmTimer(bool retain_user_task, bool is_repeating)
    : base::Timer(retain_user_task, is_repeating),
      delegate_(new RtcAlarm()),
      can_wake_from_suspend_(false),
      origin_message_loop_(NULL),
      weak_factory_(this) {
  can_wake_from_suspend_ = delegate_->Init(weak_factory_.GetWeakPtr());
}

AlarmTimer::AlarmTimer(const tracked_objects::Location& posted_from,
                       base::TimeDelta delay,
                       const base::Closure& user_task,
                       bool is_repeating)
    : base::Timer(posted_from, delay, user_task, is_repeating),
      delegate_(new RtcAlarm()),
      can_wake_from_suspend_(false),
      origin_message_loop_(NULL),
      weak_factory_(this) {
  can_wake_from_suspend_ = delegate_->Init(weak_factory_.GetWeakPtr());
}

AlarmTimer::~AlarmTimer() {
  Stop();
}

void AlarmTimer::OnTimerFired() {
  if (!base::Timer::IsRunning())
    return;

  DCHECK(pending_task_.get());

  // Take ownership of the pending user task, which is going to be cleared by
  // the Stop() or Reset() functions below.
  scoped_ptr<base::PendingTask> pending_user_task(pending_task_.Pass());

  // Re-schedule or stop the timer as requested.
  if (base::Timer::is_repeating())
    Reset();
  else
    Stop();

  // Now run the user task.
  base::MessageLoop::current()->task_annotator()->RunTask(
      "AlarmTimer::Reset", "AlarmTimer::OnTimerFired", *pending_user_task);
}

void AlarmTimer::Stop() {
  if (!can_wake_from_suspend_) {
    base::Timer::Stop();
    return;
  }

  // Clear the running flag, stop the delegate, and delete the pending task.
  base::Timer::set_is_running(false);
  delegate_->Stop();
  pending_task_.reset();

  // Stop is called when the AlarmTimer is destroyed so we need to remove
  // ourselves as a MessageLoop::DestructionObserver to prevent a segfault
  // later.
  if (origin_message_loop_) {
    origin_message_loop_->RemoveDestructionObserver(this);
    origin_message_loop_ = NULL;
  }

  if (!base::Timer::retain_user_task())
    base::Timer::set_user_task(base::Closure());
}

void AlarmTimer::Reset() {
  if (!can_wake_from_suspend_) {
    base::Timer::Reset();
    return;
  }

  DCHECK(!base::Timer::user_task().is_null());
  DCHECK(!origin_message_loop_ ||
         origin_message_loop_->task_runner()->RunsTasksOnCurrentThread());

  // Make sure that the timer will stop if the underlying message loop is
  // destroyed.
  if (!origin_message_loop_) {
    origin_message_loop_ = base::MessageLoop::current();
    origin_message_loop_->AddDestructionObserver(this);
  }

  // Set up the pending task.
  if (base::Timer::GetCurrentDelay() > base::TimeDelta::FromMicroseconds(0)) {
    base::Timer::set_desired_run_time(
        base::TimeTicks::Now() + base::Timer::GetCurrentDelay());
    pending_task_.reset(new base::PendingTask(base::Timer::posted_from(),
                                              base::Timer::user_task(),
                                              base::Timer::desired_run_time(),
                                              true  /* nestable */));
  } else {
    base::Timer::set_desired_run_time(base::TimeTicks());
    pending_task_.reset(new base::PendingTask(base::Timer::posted_from(),
                                              base::Timer::user_task()));
  }
  base::MessageLoop::current()->task_annotator()->DidQueueTask(
      "AlarmTimer::Reset", *pending_task_);

  // Now start up the timer.
  delegate_->Reset(base::Timer::GetCurrentDelay());
  base::Timer::set_is_running(true);
}

void AlarmTimer::WillDestroyCurrentMessageLoop() {
  Stop();
}

}  // namespace timers
