// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/command_buffer_task_runner.h"

#include "base/bind.h"
#include "base/thread_task_runner_handle.h"
#include "components/mus/gles2/command_buffer_driver.h"

namespace mus {

CommandBufferTaskRunner::CommandBufferTaskRunner()
    : driver_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      need_post_task_(true),
      cond_(&lock_) {
}

bool CommandBufferTaskRunner::PostTask(
    const CommandBufferDriver* driver,
    const TaskCallback& task) {
  base::AutoLock locker(lock_);
  driver_map_[driver].push_back(task);
  if (driver->IsScheduled()) {
    ScheduleTaskIfNecessaryLocked();
    // A new task has been submitted, and we need singal the main thread.
    // If the main thread is waiting for it, it will resume executing tasks.
    cond_.Signal();
  }
  return true;
}

void CommandBufferTaskRunner::RunOneTask() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock locker(lock_);
  const bool block = true;
  bool result = RunOneTaskInternalLocked(block);
  DCHECK(result);
}

void CommandBufferTaskRunner::OnScheduled(const CommandBufferDriver* driver) {
  base::AutoLock locker(lock_);
  if (driver_map_.find(driver) != driver_map_.end()) {
    ScheduleTaskIfNecessaryLocked();
    // Some task submitted from this |driver| become executable, and we need
    // singal the main thread. If the main thread is waiting for it, it will
    // resume executing tasks.
    cond_.Signal();
  }
}

CommandBufferTaskRunner::~CommandBufferTaskRunner() {}

bool CommandBufferTaskRunner::RunOneTaskInternalLocked(bool block) {
  DCHECK(thread_checker_.CalledOnValidThread());
  lock_.AssertAcquired();
  do {
    for (auto it = driver_map_.begin(); it != driver_map_.end(); ++it) {
      if (!it->first->IsScheduled())
        continue;
      TaskQueue& task_queue = it->second;
      DCHECK(!task_queue.empty());
      const TaskCallback& callback = task_queue.front();
      bool complete = false;
      {
        base::AutoUnlock unlocker(lock_);
        complete = callback.Run();
      }
      if (complete) {
        // Only remove the task if it is complete.
        task_queue.pop_front();
        if (task_queue.empty())
          driver_map_.erase(it);
      }
      return true;
    }
    // If we cannot find any executable task and block is true, we have to wait
    // until other threads submit a new task or some tasks become executable.
    if (block)
      cond_.Wait();
  } while (block);

  return false;
}

void CommandBufferTaskRunner::ScheduleTaskIfNecessaryLocked() {
  lock_.AssertAcquired();
  if (!need_post_task_)
    return;
  driver_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CommandBufferTaskRunner::RunCommandBufferTask, this));
  need_post_task_ = false;
}

void CommandBufferTaskRunner::RunCommandBufferTask() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock locker(lock_);

  // Try executing all tasks in |driver_map_| until |RunOneTaskInternalLock()|
  // returns false (Returning false means |driver_map_| is empty or all tasks
  // in it aren't executable currently).
  const bool block = false;
  while (RunOneTaskInternalLocked(block));
  need_post_task_ = true;
}

}  // namespace mus
