// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/task_marshaller.h"
#include "base/task.h"

TaskMarshallerThroughMessageQueue::TaskMarshallerThroughMessageQueue()
    : wnd_(NULL),
      msg_(0xFFFF) {
}

TaskMarshallerThroughMessageQueue::~TaskMarshallerThroughMessageQueue() {
  ClearTasks();
}

void TaskMarshallerThroughMessageQueue::PostTask(
    const tracked_objects::Location& from_here, const base::Closure& task) {
  DCHECK(wnd_ != NULL);
  lock_.Acquire();
  bool has_work = !pending_tasks_.empty();
  pending_tasks_.push(task);
  lock_.Release();

  // Don't post message if there is already one.
  if (has_work)
    return;

  if (!::PostMessage(wnd_, msg_, 0, 0)) {
    DVLOG(1) << "Dropping MSG_EXECUTE_TASK message for destroyed window.";
    ClearTasks();
  }
}

void TaskMarshallerThroughMessageQueue::PostDelayedTask(
    const tracked_objects::Location& source,
    const base::Closure& task,
    base::TimeDelta& delay) {
  DCHECK(wnd_);

  base::AutoLock lock(lock_);
  base::PendingTask delayed_task(source, task, base::TimeTicks::Now() + delay,
                                 true);
  base::TimeTicks top_run_time = delayed_tasks_.top().delayed_run_time;
  delayed_tasks_.push(delayed_task);

  // Reschedule the timer if |delayed_task| will be the next delayed task to
  // run.
  if (delayed_task.delayed_run_time < top_run_time) {
    ::SetTimer(wnd_, reinterpret_cast<UINT_PTR>(this),
               static_cast<DWORD>(delay.InMilliseconds()), NULL);
  }
}

BOOL TaskMarshallerThroughMessageQueue::ProcessWindowMessage(HWND hWnd,
                                                             UINT uMsg,
                                                             WPARAM wParam,
                                                             LPARAM lParam,
                                                             LRESULT& lResult,
                                                             DWORD dwMsgMapID) {
  if (hWnd == wnd_ && uMsg == msg_) {
    ExecuteQueuedTasks();
    lResult = 0;
    return TRUE;
  }

  if (hWnd == wnd_ && uMsg == WM_TIMER) {
    ExecuteDelayedTasks();
    lResult = 0;
    return TRUE;
  }

  return FALSE;
}

base::Closure TaskMarshallerThroughMessageQueue::PopTask() {
  base::AutoLock lock(lock_);
  if (pending_tasks_.empty())
    return base::Closure();

  base::Closure task = pending_tasks_.front();
  pending_tasks_.pop();
  return task;
}

void TaskMarshallerThroughMessageQueue::ExecuteQueuedTasks() {
  DCHECK(CalledOnValidThread());
  base::Closure task;
  while (!(task = PopTask()).is_null())
    task.Run();
}

void TaskMarshallerThroughMessageQueue::ExecuteDelayedTasks() {
  DCHECK(CalledOnValidThread());
  ::KillTimer(wnd_, reinterpret_cast<UINT_PTR>(this));
  while (true) {
    lock_.Acquire();

    if (delayed_tasks_.empty()) {
      lock_.Release();
      return;
    }

    base::PendingTask next_task = delayed_tasks_.top();
    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeTicks next_run = next_task.delayed_run_time;
    if (next_run > now) {
      int64 delay = (next_run - now).InMillisecondsRoundedUp();
      ::SetTimer(wnd_, reinterpret_cast<UINT_PTR>(this),
                 static_cast<DWORD>(delay), NULL);
      lock_.Release();
      return;
    }

    delayed_tasks_.pop();
    lock_.Release();

    // Run the task outside the lock.
    next_task.task.Run();
  }
}

void TaskMarshallerThroughMessageQueue::ClearTasks() {
  base::AutoLock lock(lock_);
  DVLOG_IF(1, !pending_tasks_.empty()) << "Destroying "
                                       << pending_tasks_.size()
                                       << " pending tasks.";
  while (!pending_tasks_.empty())
    pending_tasks_.pop();

  while (!delayed_tasks_.empty())
    delayed_tasks_.pop();
}
