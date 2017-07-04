// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/task_service.h"

#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"

namespace midi {

namespace {

constexpr TaskService::InstanceId kInvalidInstanceId = -1;

}  // namespace

TaskService::TaskService()
    : next_instance_id_(0), bound_instance_id_(kInvalidInstanceId) {}

TaskService::~TaskService() {
  std::vector<std::unique_ptr<base::Thread>> threads;
  {
    base::AutoLock lock(lock_);
    threads = std::move(threads_);
    DCHECK_EQ(kInvalidInstanceId, bound_instance_id_);
  }
  // Should not have any lock to perform thread joins on thread destruction.
  // All posted tasks should run before quitting the thread message loop.
  threads.clear();
}

bool TaskService::BindInstance() {
  base::AutoLock lock(lock_);
  if (bound_instance_id_ != kInvalidInstanceId)
    return false;
  bound_instance_id_ = next_instance_id_++;

  DCHECK(!default_task_runner_);
  default_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  return true;
}

bool TaskService::UnbindInstance() {
  {
    base::AutoLock lock(lock_);
    if (bound_instance_id_ == kInvalidInstanceId)
      return false;
    bound_instance_id_ = kInvalidInstanceId;

    DCHECK(default_task_runner_);
    default_task_runner_ = nullptr;
  }
  // From now on RunTask will never run any task bound to the instance id.
  // But invoked tasks might be still running here. To ensure no task run on
  // quitting this method, take writer lock of |task_lock_|.
  base::subtle::AutoWriteLock task_lock(task_lock_);
  return true;
}

void TaskService::PostStaticTask(RunnerId runner_id, base::OnceClosure task) {
  {
    // Disallow to post a task when no instance is bound, so that new threads
    // can not be created after the thread finalization in the destructor.
    base::AutoLock lock(lock_);
    if (bound_instance_id_ == kInvalidInstanceId)
      return;
  }
  scoped_refptr<base::SingleThreadTaskRunner> runner;
  GetTaskRunner(runner_id)->PostTask(FROM_HERE, std::move(task));
}

void TaskService::PostBoundTask(RunnerId runner_id, base::OnceClosure task) {
  InstanceId instance_id;
  {
    base::AutoLock lock(lock_);
    if (bound_instance_id_ == kInvalidInstanceId)
      return;
    instance_id = bound_instance_id_;
  }
  GetTaskRunner(runner_id)->PostTask(
      FROM_HERE, base::BindOnce(&TaskService::RunTask, base::Unretained(this),
                                instance_id, runner_id, std::move(task)));
}

void TaskService::PostBoundDelayedTask(RunnerId runner_id,
                                       base::OnceClosure task,
                                       base::TimeDelta delay) {
  InstanceId instance_id;
  {
    base::AutoLock lock(lock_);
    if (bound_instance_id_ == kInvalidInstanceId)
      return;
    instance_id = bound_instance_id_;
  }
  GetTaskRunner(runner_id)->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TaskService::RunTask, base::Unretained(this), instance_id,
                     runner_id, std::move(task)),
      delay);
}

scoped_refptr<base::SingleThreadTaskRunner> TaskService::GetTaskRunner(
    RunnerId runner_id) {
  base::AutoLock lock(lock_);
  if (runner_id == kDefaultRunnerId)
    return default_task_runner_;

  if (threads_.size() < runner_id)
    threads_.resize(runner_id);

  size_t thread = runner_id - 1;
  if (!threads_[thread]) {
    threads_[thread] = base::MakeUnique<base::Thread>(
        base::StringPrintf("MidiService_TaskService_Thread(%zu)", runner_id));
#if defined(OS_WIN)
    threads_[thread]->init_com_with_mta(true);
#endif
    threads_[thread]->Start();
  }
  return threads_[thread]->task_runner();
}

void TaskService::RunTask(InstanceId instance_id,
                          RunnerId runner_id,
                          base::OnceClosure task) {
  base::subtle::AutoReadLock task_lock(task_lock_);
  {
    base::AutoLock lock(lock_);
    // If UnbindInstance() is already called, do nothing.
    if (instance_id != bound_instance_id_)
      return;
  }
  std::move(task).Run();
}

}  // namespace midi
