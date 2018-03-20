// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/startup_task_runner.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"

namespace content {

StartupTaskRunner::StartupTaskRunner(
    base::Callback<void(int)> const startup_complete_callback,
    scoped_refptr<base::SingleThreadTaskRunner> proxy)
    : startup_complete_callback_(startup_complete_callback), proxy_(proxy) {}

StartupTaskRunner::~StartupTaskRunner() {}

void StartupTaskRunner::AddTask(StartupTask& callback) {
  task_list_.push_back(callback);
}

void StartupTaskRunner::StartRunningTasksAsync() {
  DCHECK(proxy_.get());
  int result = 0;
  if (task_list_.empty()) {
    if (!startup_complete_callback_.is_null()) {
      startup_complete_callback_.Run(result);
      // Clear the callback to prevent it being called a second time
      startup_complete_callback_.Reset();
    }
  } else {
    const base::Closure next_task =
        base::Bind(&StartupTaskRunner::WrappedTask, base::Unretained(this));
    proxy_->PostNonNestableTask(FROM_HERE, next_task);
  }
}

void StartupTaskRunner::RunAllTasksNow() {
  int result = 0;
  for (std::list<StartupTask>::iterator it = task_list_.begin();
       it != task_list_.end();
       it++) {
    result = it->Run();
    if (result > 0) break;
  }
  task_list_.clear();
  if (!startup_complete_callback_.is_null()) {
    startup_complete_callback_.Run(result);
    // Clear the callback to prevent it being called a second time
    startup_complete_callback_.Reset();
  }
}

void StartupTaskRunner::WrappedTask() {
  if (task_list_.empty()) {
    // This will happen if the remaining tasks have been run synchronously since
    // the WrappedTask was created. Any callback will already have been called,
    // so there is nothing to do
    return;
  }
  int result = task_list_.front().Run();
  task_list_.pop_front();
  if (result > 0) {
    // Stop now and throw away the remaining tasks
    task_list_.clear();
  }
  if (task_list_.empty()) {
    if (!startup_complete_callback_.is_null()) {
      startup_complete_callback_.Run(result);
      // Clear the callback to prevent it being called a second time
      startup_complete_callback_.Reset();
    }
  } else {
    const base::Closure next_task =
        base::Bind(&StartupTaskRunner::WrappedTask, base::Unretained(this));
    proxy_->PostNonNestableTask(FROM_HERE, next_task);
  }
}

}  // namespace content
