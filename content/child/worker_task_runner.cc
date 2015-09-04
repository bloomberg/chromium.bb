// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/worker_task_runner.h"

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "content/child/worker_thread_impl.h"

namespace content {

namespace {

// A task-runner that refuses to run any tasks.
class DoNothingTaskRunner : public base::TaskRunner {
 public:
  DoNothingTaskRunner() {}

 private:
  ~DoNothingTaskRunner() override {}

  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override {
    return false;
  }

  bool RunsTasksOnCurrentThread() const override { return false; }
};

}  // namespace

WorkerTaskRunner::WorkerTaskRunner()
    : task_runner_for_dead_worker_(new DoNothingTaskRunner()) {
}

bool WorkerTaskRunner::PostTask(
    int id, const base::Closure& closure) {
  DCHECK(id > 0);
  base::AutoLock locker(task_runner_map_lock_);
  IDToTaskRunnerMap::iterator found = task_runner_map_.find(id);
  if (found == task_runner_map_.end())
    return false;
  return found->second->PostTask(FROM_HERE, closure);
}

int WorkerTaskRunner::PostTaskToAllThreads(const base::Closure& closure) {
  base::AutoLock locker(task_runner_map_lock_);
  for (const auto& it : task_runner_map_)
    it.second->PostTask(FROM_HERE, closure);
  return static_cast<int>(task_runner_map_.size());
}

WorkerTaskRunner* WorkerTaskRunner::Instance() {
  static base::LazyInstance<WorkerTaskRunner>::Leaky
      worker_task_runner = LAZY_INSTANCE_INITIALIZER;
  return worker_task_runner.Pointer();
}

WorkerTaskRunner::~WorkerTaskRunner() {
}

void WorkerTaskRunner::DidStartWorkerRunLoop() {
  DCHECK(!base::PlatformThread::CurrentRef().is_null());
  // Note: call into WorkerThreadImpl rather than this class observing
  // WorkerThreadImpl, to ensure that the task runner exists before anybody is
  // notified that the worker thread has started.
  WorkerThreadImpl::DidStartCurrentWorkerThread();
  int id = base::PlatformThread::CurrentId();
  base::AutoLock locker_(task_runner_map_lock_);
  task_runner_map_[id] = base::ThreadTaskRunnerHandle::Get().get();
  CHECK(task_runner_map_[id]);
}

void WorkerTaskRunner::WillStopWorkerRunLoop() {
  WorkerThreadImpl::WillStopCurrentWorkerThread();
  {
    base::AutoLock locker(task_runner_map_lock_);
    task_runner_map_.erase(WorkerThread::GetCurrentId());
  }
}

base::TaskRunner* WorkerTaskRunner::GetTaskRunnerFor(int worker_id) {
  base::AutoLock locker(task_runner_map_lock_);
  return ContainsKey(task_runner_map_, worker_id) ? task_runner_map_[worker_id]
                                           : task_runner_for_dead_worker_.get();
}

}  // namespace content
