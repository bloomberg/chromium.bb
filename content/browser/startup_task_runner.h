// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STARTUP_TASK_RUNNER_H_
#define CONTENT_BROWSER_STARTUP_TASK_RUNNER_H_

#include <list>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"

#include "build/build_config.h"

#include "content/public/browser/browser_main_runner.h"

namespace content {

// A startup task is a void function returning the status on completion.
// a status of > 0 indicates a failure, and that no further startup tasks should
// be run.
typedef base::Callback<int(void)> StartupTask;

// This class runs startup tasks. The tasks are either run immediately inline,
// or are queued one at a time on the UI thread's message loop. If the events
// are queued, UI events that are received during startup will be acted upon
// between startup tasks. The motivation for this is that, on targets where the
// UI is already started, it allows us to keep the UI responsive during startup.
//
// Note that this differs from a SingleThreadedTaskRunner in that there may be
// no opportunity to handle UI events between the tasks of a
// SingleThreadedTaskRunner.

class CONTENT_EXPORT StartupTaskRunner
    : public base::RefCounted<StartupTaskRunner> {

 public:
  // Constructor: Note that |startup_complete_callback| is optional. If it is
  // not null it will be called once all the startup tasks have run.
  StartupTaskRunner(bool browser_may_start_asynchronously,
                    base::Callback<void(int)> startup_complete_callback,
                    scoped_refptr<base::SingleThreadTaskRunner> proxy);

  // Add a task to the queue of startup tasks to be run.
  virtual void AddTask(StartupTask& callback);

  // Start running the tasks.
  virtual void StartRunningTasks();

 private:
  friend class base::RefCounted<StartupTaskRunner>;
  virtual ~StartupTaskRunner();

  std::list<StartupTask> task_list_;
  void WrappedTask();

  const bool asynchronous_startup_;
  base::Callback<void(int)> startup_complete_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> proxy_;

  DISALLOW_COPY_AND_ASSIGN(StartupTaskRunner);
};

}  // namespace content
#endif  // CONTENT_BROWSER_STARTUP_TASK_RUNNER_H_
