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
    : next_instance_id_(0), bound_instance_id_(kInvalidInstanceId) {
  thread_task_locks_.resize(1);
  thread_task_locks_[0] = base::MakeUnique<base::Lock>();

  // We do not need a dedicated thread for the default runner since it uses
  // the thread that calls BindInstance(), but we allocates threads_ and keeps
  // threads_[0] empty to use the same index between |thread_task_locks_| and
  // |threads_| for readability.
  threads_.resize(1);
}

TaskService::~TaskService() {
  base::AutoLock lock(lock_);
  threads_.clear();
}

bool TaskService::BindInstance() {
  {
    base::AutoLock instance_lock(instance_lock_);
    if (bound_instance_id_ != kInvalidInstanceId)
      return false;
    bound_instance_id_ = next_instance_id_++;
  }
  base::AutoLock lock(lock_);
  DCHECK(!default_task_runner_);
  default_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  return true;
}

bool TaskService::UnbindInstance() {
  {
    base::AutoLock instance_lock(instance_lock_);
    if (bound_instance_id_ == kInvalidInstanceId)
      return false;
    bound_instance_id_ = kInvalidInstanceId;
  }
  base::AutoLock lock(lock_);
  DCHECK(default_task_runner_);
  default_task_runner_ = nullptr;
  // From now on RunTask will never run any task bound to the instance id.
  // But invoked tasks might be still running here. To ensure no task run before
  // quitting this method, take all |thread_task_locks_| once.
  for (auto& task_lock : thread_task_locks_)
    base::AutoLock auto_task_lock(*task_lock);
  return true;
}

void TaskService::PostStaticTask(RunnerId runner_id, base::OnceClosure task) {
  scoped_refptr<base::SingleThreadTaskRunner> runner;
  {
    base::AutoLock lock(lock_);
    runner = GetTaskRunner(runner_id);
  }
  runner->PostTask(FROM_HERE, std::move(task));
}

void TaskService::PostBoundTask(RunnerId runner_id, base::OnceClosure task) {
  base::AutoLock instance_lock(instance_lock_);
  if (bound_instance_id_ == kInvalidInstanceId)
    return;
  scoped_refptr<base::SingleThreadTaskRunner> runner;
  InstanceId instance_id;
  {
    base::AutoLock lock(lock_);
    runner = GetTaskRunner(runner_id);
    instance_id = bound_instance_id_;
  }
  runner->PostTask(FROM_HERE,
                   base::BindOnce(&TaskService::RunTask, base::Unretained(this),
                                  instance_id, runner_id, std::move(task)));
}

void TaskService::PostBoundDelayedTask(RunnerId runner_id,
                                       base::OnceClosure task,
                                       base::TimeDelta delay) {
  base::AutoLock instance_lock(instance_lock_);
  if (bound_instance_id_ == kInvalidInstanceId)
    return;
  scoped_refptr<base::SingleThreadTaskRunner> runner;
  InstanceId instance_id;
  {
    base::AutoLock lock(lock_);
    runner = GetTaskRunner(runner_id);
    instance_id = bound_instance_id_;
  }
  runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TaskService::RunTask, base::Unretained(this), instance_id,
                     runner_id, std::move(task)),
      delay);
}

scoped_refptr<base::SingleThreadTaskRunner> TaskService::GetTaskRunner(
    RunnerId runner_id) {
  lock_.AssertAcquired();
  if (runner_id == kDefaultRunnerId)
    return default_task_runner_;

  DCHECK_EQ(threads_.size(), thread_task_locks_.size());

  if (threads_.size() <= runner_id) {
    threads_.resize(runner_id + 1);
    thread_task_locks_.resize(runner_id + 1);
  }
  if (!threads_[runner_id]) {
    threads_[runner_id] = base::MakeUnique<base::Thread>(
        base::StringPrintf("MidiService_TaskService_Thread(%zu)", runner_id));
#if defined(OS_WIN)
    threads_[runner_id]->init_com_with_mta(true);
#endif
    threads_[runner_id]->Start();

    DCHECK(!thread_task_locks_[runner_id]);
    thread_task_locks_[runner_id] = base::MakeUnique<base::Lock>();
  }
  return threads_[runner_id]->task_runner();
}

void TaskService::RunTask(InstanceId instance_id,
                          RunnerId runner_id,
                          base::OnceClosure task) {
  std::unique_ptr<base::AutoLock> task_lock;
  {
    base::AutoLock instance_lock(instance_lock_);
    // If UnbindInstance() is already called, do nothing.
    if (instance_id != bound_instance_id_)
      return;

    // Obtains task lock to ensure that the instance should not complete
    // UnbindInstance() while running the |task|.
    base::AutoLock lock(lock_);
    task_lock =
        base::MakeUnique<base::AutoLock>(*thread_task_locks_[runner_id]);
  }
  std::move(task).Run();
}

}  // namespace midi
