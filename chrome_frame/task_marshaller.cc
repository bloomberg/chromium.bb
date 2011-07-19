// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/task_marshaller.h"
#include "base/task.h"

TaskMarshallerThroughMessageQueue::TaskMarshallerThroughMessageQueue() {
  wnd_ = NULL;
  msg_ = 0xFFFF;
}

TaskMarshallerThroughMessageQueue::~TaskMarshallerThroughMessageQueue() {
  DeleteAll();
}

void TaskMarshallerThroughMessageQueue::PostTask(
    const tracked_objects::Location& from_here, Task* task) {
  DCHECK(wnd_ != NULL);
  task->SetBirthPlace(from_here);
  lock_.Acquire();
  bool has_work = !pending_tasks_.empty();
  pending_tasks_.push(task);
  lock_.Release();

  // Don't post message if there is already one.
  if (has_work)
    return;

  if (!::PostMessage(wnd_, msg_, 0, 0)) {
    DVLOG(1) << "Dropping MSG_EXECUTE_TASK message for destroyed window.";
    DeleteAll();
  }
}

void TaskMarshallerThroughMessageQueue::PostDelayedTask(
    const tracked_objects::Location& source,
    Task* task,
    base::TimeDelta& delay) {
  DCHECK(wnd_ != NULL);
  base::AutoLock lock(lock_);
  DelayedTask delayed_task(task, base::Time::Now() + delay);
  delayed_tasks_.push(delayed_task);
  // If we become the 'top' task - reschedule the timer.
  if (delayed_tasks_.top().task == task) {
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

Task* TaskMarshallerThroughMessageQueue::PopTask() {
  base::AutoLock lock(lock_);
  Task* task = NULL;
  if (!pending_tasks_.empty()) {
    task = pending_tasks_.front();
    pending_tasks_.pop();
  }
  return task;
}

void TaskMarshallerThroughMessageQueue::ExecuteQueuedTasks() {
  DCHECK(CalledOnValidThread());
  Task* task;
  while ((task = PopTask()) != NULL) {
    RunTask(task);
  }
}

void TaskMarshallerThroughMessageQueue::ExecuteDelayedTasks() {
  DCHECK(CalledOnValidThread());
  ::KillTimer(wnd_, reinterpret_cast<UINT_PTR>(this));
  while (1) {
    lock_.Acquire();

    if (delayed_tasks_.empty()) {
      lock_.Release();
      return;
    }

    base::Time now = base::Time::Now();
    DelayedTask next_task = delayed_tasks_.top();
    base::Time next_run = next_task.run_at;
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
    RunTask(next_task.task);
  }
}

void TaskMarshallerThroughMessageQueue::DeleteAll() {
  base::AutoLock lock(lock_);
  DVLOG_IF(1, !pending_tasks_.empty()) << "Destroying "
                                       << pending_tasks_.size()
                                       << " pending tasks.";
  while (!pending_tasks_.empty()) {
    Task* task = pending_tasks_.front();
    pending_tasks_.pop();
    delete task;
  }

  while (!delayed_tasks_.empty()) {
    delete delayed_tasks_.top().task;
    delayed_tasks_.pop();
  }
}

void TaskMarshallerThroughMessageQueue::RunTask(Task* task) {
  ++invoke_task_;
  task->Run();
  --invoke_task_;
  delete task;
}

bool TaskMarshallerThroughMessageQueue::DelayedTask::operator<(
    const DelayedTask& other) const {
  // Since the top of a priority queue is defined as the "greatest" element, we
  // need to invert the comparison here.  We want the smaller time to be at the
  // top of the heap.
  if (run_at < other.run_at)
    return false;

  if (run_at > other.run_at)
    return true;

  // If the times happen to match, then we use the sequence number to decide.
  // Compare the difference to support integer roll-over.
  return (seq - other.seq) > 0;
}
