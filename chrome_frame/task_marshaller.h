// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TASK_MARSHALLER_H_
#define CHROME_FRAME_TASK_MARSHALLER_H_
#pragma once

#include <windows.h>
#include <deque>
#include <queue>

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
  ~TaskMarshallerThroughMessageQueue();

  void SetWindow(HWND wnd, UINT msg) {
    wnd_ = wnd;
    msg_ = msg;
  }

  virtual void PostTask(const tracked_objects::Location& from_here,
                        Task* task);
  virtual void PostDelayedTask(const tracked_objects::Location& source,
                               Task* task,
                               base::TimeDelta& delay);
  // Called by the owner of the HWND.
  BOOL ProcessWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                            LRESULT& lResult, DWORD dwMsgMapID = 0);
 private:
  void DeleteAll();
  inline Task* PopTask();
  inline void ExecuteQueuedTasks();
  void ExecuteDelayedTasks();
  void RunTask(Task* task);

  struct DelayedTask {
    DelayedTask(Task* task, base::Time at) : run_at(at), task(task), seq(0) {}
    base::Time run_at;
    Task* task;
    int seq;
    // To support sorting based on time in priority_queue.
    bool operator<(const DelayedTask& other) const;
  };

  std::priority_queue<DelayedTask> delayed_tasks_;
  std::queue<Task*> pending_tasks_;
  base::Lock lock_;
  HWND wnd_;
  UINT msg_;
  int invoke_task_;
};

#endif  // CHROME_FRAME_TASK_MARSHALLER_H_
