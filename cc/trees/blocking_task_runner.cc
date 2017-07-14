// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/blocking_task_runner.h"

#include <stddef.h>

#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace cc {

// static
std::unique_ptr<BlockingTaskRunner> BlockingTaskRunner::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return base::WrapUnique(new BlockingTaskRunner(task_runner));
}

BlockingTaskRunner::BlockingTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : thread_id_(base::PlatformThread::CurrentId()),
      task_runner_(task_runner),
      capture_(0) {
}

BlockingTaskRunner::~BlockingTaskRunner() {}

bool BlockingTaskRunner::BelongsToCurrentThread() {
  return base::PlatformThread::CurrentId() == thread_id_;
}

bool BlockingTaskRunner::PostTask(const tracked_objects::Location& from_here,
                                  base::OnceClosure task) {
  base::AutoLock lock(lock_);
  DCHECK(task_runner_.get() || capture_);
  if (!capture_)
    return task_runner_->PostTask(from_here, std::move(task));
  captured_tasks_.push_back(std::move(task));
  return true;
}

void BlockingTaskRunner::SetCapture(bool capture) {
  DCHECK(BelongsToCurrentThread());

  std::vector<base::OnceClosure> tasks;

  {
    base::AutoLock lock(lock_);
    capture_ += capture ? 1 : -1;
    DCHECK_GE(capture_, 0);

    if (capture_)
      return;

    // We're done capturing, so grab all the captured tasks and run them.
    tasks.swap(captured_tasks_);
  }
  for (size_t i = 0; i < tasks.size(); ++i)
    std::move(tasks[i]).Run();
}

BlockingTaskRunner::CapturePostTasks::CapturePostTasks(
    BlockingTaskRunner* blocking_runner)
    : blocking_runner_(blocking_runner) {
  blocking_runner_->SetCapture(true);
}

BlockingTaskRunner::CapturePostTasks::~CapturePostTasks() {
  blocking_runner_->SetCapture(false);
}

}  // namespace cc
