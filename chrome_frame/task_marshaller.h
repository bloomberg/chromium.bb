// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TASK_MARSHALLER_H_
#define CHROME_FRAME_TASK_MARSHALLER_H_
#pragma once

#include <windows.h>
#include <deque>
#include <queue>

#include "base/callback.h"
#include "base/pending_task.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"

class Task;
namespace tracked_objects {
  class Location;
}

// TaskMarshallerThroughMessageQueue is similar to base::MessageLoopForUI
// in cases where we do not control the thread lifetime and message retrieval
// and dispatching. It uses a HWND to ::PostMessage to it as a signal that
// the task queue is not empty.
class TaskMarshallerThroughMessageQueue : public base::NonThreadSafe {
 public:
  TaskMarshallerThroughMessageQueue();
  virtual ~TaskMarshallerThroughMessageQueue();

  void SetWindow(HWND wnd, UINT msg) {
    wnd_ = wnd;
    msg_ = msg;
  }

  virtual void PostTask(const tracked_objects::Location& from_here,
                        const base::Closure& task);
  virtual void PostDelayedTask(const tracked_objects::Location& source,
                               const base::Closure& task,
                               base::TimeDelta& delay);

  // Called by the owner of the HWND.
  BOOL ProcessWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                            LRESULT& lResult, DWORD dwMsgMapID = 0);
 private:
  void ClearTasks();
  inline base::Closure PopTask();
  inline void ExecuteQueuedTasks();
  void ExecuteDelayedTasks();

  // Shortest delays ordered at the top of the queue.
  base::DelayedTaskQueue delayed_tasks_;

  // A list of tasks that need to be processed by this instance.
  std::queue<base::Closure> pending_tasks_;

  // Lock accesses to |pending_tasks_|.
  base::Lock lock_;

  // ::PostMessage parameters.
  HWND wnd_;
  UINT msg_;
};

#endif  // CHROME_FRAME_TASK_MARSHALLER_H_
