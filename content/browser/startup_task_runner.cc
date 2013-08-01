// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/startup_task_runner.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"

namespace content {

StartupTaskRunner::StartupTaskRunner(
    bool browser_may_start_asynchronously,
    base::Callback<void(int)> const startup_complete_callback,
    scoped_refptr<base::SingleThreadTaskRunner> proxy)
    : asynchronous_startup_(browser_may_start_asynchronously),
      startup_complete_callback_(startup_complete_callback),
      proxy_(proxy) {}

void StartupTaskRunner::AddTask(StartupTask& callback) {
  task_list_.push_back(callback);
}

void StartupTaskRunner::StartRunningTasks() {
  DCHECK(proxy_);
  int result = 0;
  if (asynchronous_startup_ && !task_list_.empty()) {
    const base::Closure next_task =
        base::Bind(&StartupTaskRunner::WrappedTask, this);
    proxy_->PostNonNestableTask(FROM_HERE, next_task);
  } else {
    for (std::list<StartupTask>::iterator it = task_list_.begin();
         it != task_list_.end();
         it++) {
      result = it->Run();
      if (result > 0) {
        break;
      }
    }
    if (!startup_complete_callback_.is_null()) {
      startup_complete_callback_.Run(result);
    }
  }
}

void StartupTaskRunner::WrappedTask() {
  int result = task_list_.front().Run();
  task_list_.pop_front();
  if (result > 0 || task_list_.empty()) {
    if (!startup_complete_callback_.is_null()) {
      startup_complete_callback_.Run(result);
    }
  } else {
    const base::Closure next_task =
        base::Bind(&StartupTaskRunner::WrappedTask, this);
    proxy_->PostNonNestableTask(FROM_HERE, next_task);
  }
}

StartupTaskRunner::~StartupTaskRunner() {}

}  // namespace content
