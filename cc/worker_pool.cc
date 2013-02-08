// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/worker_pool.h"

#include <algorithm>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"

#if defined(OS_ANDROID)
// TODO(epenner): Move thread priorities to base. (crbug.com/170549)
#include <sys/resource.h>
#endif

namespace cc {

namespace {

class WorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  WorkerPoolTaskImpl(const WorkerPool::Callback& task,
                     const base::Closure& reply)
      : internal::WorkerPoolTask(reply),
        task_(task) {}

  virtual void Run(RenderingStats* rendering_stats) OVERRIDE {
    task_.Run(rendering_stats);
  }

 private:
  WorkerPool::Callback task_;
};

const char* kWorkerThreadNamePrefix = "Compositor";

// Allow two pending tasks per worker. This keeps resource usage
// low while making sure workers aren't unnecessarily idle.
const int kNumPendingTasksPerWorker = 2;

}  // namespace

namespace internal {

WorkerPoolTask::WorkerPoolTask(const base::Closure& reply)
    : reply_(reply) {
}

WorkerPoolTask::~WorkerPoolTask() {
}

void WorkerPoolTask::Completed() {
  reply_.Run();
}

}  // namespace internal

WorkerPool::Worker::Worker(WorkerPool* worker_pool, const std::string name)
    : base::Thread(name.c_str()),
      worker_pool_(worker_pool),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      rendering_stats_(make_scoped_ptr(new RenderingStats)),
      record_rendering_stats_(false) {
  Start();
  DCHECK(IsRunning());
}

WorkerPool::Worker::~Worker() {
  DCHECK(!IsRunning());
  DCHECK_EQ(pending_tasks_.size(), 0);
}

void WorkerPool::Worker::StopAfterCompletingAllPendingTasks() {
  // Signals the thread to exit and returns once all pending tasks have run.
  Stop();

  // Complete all pending tasks. The Stop() call above guarantees that
  // all tasks have finished running.
  while (!pending_tasks_.empty())
    OnTaskCompleted();

  // Cancel all pending replies.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void WorkerPool::Worker::PostTask(scoped_ptr<internal::WorkerPoolTask> task) {
  DCHECK_LT(num_pending_tasks(), kNumPendingTasksPerWorker);

  RenderingStats* stats =
      record_rendering_stats_ ? rendering_stats_.get() : NULL;

  message_loop_proxy()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&Worker::RunTask,
                 base::Unretained(task.get()),
                 base::Unretained(stats)),
      base::Bind(&Worker::OnTaskCompleted, weak_ptr_factory_.GetWeakPtr()));

  pending_tasks_.push_back(task.Pass());

  worker_pool_->DidNumPendingTasksChange();
}

void WorkerPool::Worker::Init() {
#if defined(OS_ANDROID)
  // TODO(epenner): Move thread priorities to base. (crbug.com/170549)
  int nice_value = 10; // Idle priority.
  setpriority(PRIO_PROCESS, base::PlatformThread::CurrentId(), nice_value);
#endif
}

// static
void WorkerPool::Worker::RunTask(
    internal::WorkerPoolTask* task, RenderingStats* rendering_stats) {
  task->Run(rendering_stats);
}

void WorkerPool::Worker::OnTaskCompleted() {
  CHECK(!pending_tasks_.empty());

  scoped_ptr<internal::WorkerPoolTask> task = pending_tasks_.take_front();
  task->Completed();

  worker_pool_->DidNumPendingTasksChange();
}

WorkerPool::WorkerPool(size_t num_threads)
    : workers_need_sorting_(false),
      shutdown_(false) {
  const std::string thread_name_prefix = kWorkerThreadNamePrefix;
  while (workers_.size() < num_threads) {
    int thread_number = workers_.size() + 1;
    workers_.push_back(new Worker(
        this,
        thread_name_prefix + StringPrintf("Worker%d", thread_number).c_str()));
  }
}

WorkerPool::~WorkerPool() {
  Shutdown();
  STLDeleteElements(&workers_);
}

void WorkerPool::Shutdown() {
  DCHECK(!shutdown_);
  shutdown_ = true;

  for (WorkerVector::iterator it = workers_.begin();
       it != workers_.end(); it++) {
    Worker* worker = *it;
    worker->StopAfterCompletingAllPendingTasks();
  }
}

void WorkerPool::PostTaskAndReply(
    const Callback& task, const base::Closure& reply) {
  Worker* worker = GetWorkerForNextTask();

  worker->PostTask(
      make_scoped_ptr(new WorkerPoolTaskImpl(
                          task,
                          reply)).PassAs<internal::WorkerPoolTask>());
}

bool WorkerPool::IsBusy() {
  Worker* worker = GetWorkerForNextTask();

  return worker->num_pending_tasks() >= kNumPendingTasksPerWorker;
}

void WorkerPool::SetRecordRenderingStats(bool record_rendering_stats) {
  for (WorkerVector::iterator it = workers_.begin();
       it != workers_.end(); ++it) {
    Worker* worker = *it;
    worker->set_record_rendering_stats(record_rendering_stats);
  }
}

void WorkerPool::GetRenderingStats(RenderingStats* stats) {
  stats->totalRasterizeTime = base::TimeDelta();
  stats->totalPixelsRasterized = 0;
  stats->totalDeferredImageDecodeCount = 0;
  stats->totalDeferredImageDecodeTime = base::TimeDelta();
  for (WorkerVector::iterator it = workers_.begin();
       it != workers_.end(); ++it) {
    Worker* worker = *it;
    CHECK(worker->rendering_stats());
    stats->totalRasterizeTime +=
        worker->rendering_stats()->totalRasterizeTime;
    stats->totalPixelsRasterized +=
        worker->rendering_stats()->totalPixelsRasterized;
    stats->totalDeferredImageDecodeCount +=
        worker->rendering_stats()->totalDeferredImageDecodeCount;
    stats->totalDeferredImageDecodeTime +=
        worker->rendering_stats()->totalDeferredImageDecodeTime;
  }
}

WorkerPool::Worker* WorkerPool::GetWorkerForNextTask() {
  CHECK(!shutdown_);
  SortWorkersIfNeeded();
  return workers_.front();
}

void WorkerPool::DidNumPendingTasksChange() {
  workers_need_sorting_ = true;
}

void WorkerPool::SortWorkersIfNeeded() {
  if (!workers_need_sorting_)
    return;

  std::sort(workers_.begin(), workers_.end(), NumPendingTasksComparator());
  workers_need_sorting_ = false;
}

}  // namespace cc
