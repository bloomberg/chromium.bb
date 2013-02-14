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
    base::subtle::Release_Store(&completed_, 1);
  }

 private:
  WorkerPool::Callback task_;
};

const char* kWorkerThreadNamePrefix = "Compositor";

#if defined(OS_ANDROID)
const int kNumPendingTasksPerWorker = 8;
#else
const int kNumPendingTasksPerWorker = 40;
#endif

const int kCheckForCompletedTasksDelayMs = 6;

}  // namespace

namespace internal {

WorkerPoolTask::WorkerPoolTask(const base::Closure& reply) : reply_(reply) {
  base::subtle::Acquire_Store(&completed_, 0);
}

WorkerPoolTask::~WorkerPoolTask() {
}

bool WorkerPoolTask::HasCompleted() {
  return base::subtle::Acquire_Load(&completed_) == 1;
}

void WorkerPoolTask::DidComplete() {
  DCHECK_EQ(base::subtle::Acquire_Load(&completed_), 1);
  reply_.Run();
}

}  // namespace internal

WorkerPool::Worker::Worker(WorkerPool* worker_pool, const std::string name)
    : base::Thread(name.c_str()),
      worker_pool_(worker_pool),
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
}

void WorkerPool::Worker::PostTask(scoped_ptr<internal::WorkerPoolTask> task) {
  RenderingStats* stats =
      record_rendering_stats_ ? rendering_stats_.get() : NULL;

  worker_pool_->WillPostTask();

  message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&Worker::RunTask,
                 base::Unretained(task.get()),
                 base::Unretained(worker_pool_),
                 base::Unretained(stats)));

  pending_tasks_.push_back(task.Pass());
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
    internal::WorkerPoolTask* task,
    WorkerPool* worker_pool,
    RenderingStats* rendering_stats) {
  task->Run(rendering_stats);
  worker_pool->OnWorkCompletedOnWorkerThread();
}

void WorkerPool::Worker::OnTaskCompleted() {
  CHECK(!pending_tasks_.empty());

  scoped_ptr<internal::WorkerPoolTask> task = pending_tasks_.take_front();

  // Notify worker pool of task completion.
  worker_pool_->OnTaskCompleted();

  task->DidComplete();
}

void WorkerPool::Worker::CheckForCompletedTasks() {
  while (!pending_tasks_.empty()) {
    if (!pending_tasks_.front()->HasCompleted())
      return;

    OnTaskCompleted();
  }
}

WorkerPool::WorkerPool(WorkerPoolClient* client, size_t num_threads)
    : client_(client),
      origin_loop_(base::MessageLoopProxy::current()),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      workers_need_sorting_(false),
      pending_task_count_(0),
      shutdown_(false),
      check_for_completed_tasks_pending_(false),
      idle_callback_(
          base::Bind(&WorkerPool::OnIdle, weak_ptr_factory_.GetWeakPtr())) {
  const std::string thread_name_prefix = kWorkerThreadNamePrefix;
  while (workers_.size() < num_threads) {
    int thread_number = workers_.size() + 1;
    workers_.push_back(new Worker(
        this,
        thread_name_prefix + StringPrintf("Worker%d", thread_number).c_str()));
  }
  base::subtle::Acquire_Store(&pending_task_count_, 0);
}

WorkerPool::~WorkerPool() {
  Shutdown();
  STLDeleteElements(&workers_);
  // Cancel all pending callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();
  DCHECK_EQ(base::subtle::Acquire_Load(&pending_task_count_), 0);
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

void WorkerPool::ScheduleCheckForCompletedTasks() {
  if (check_for_completed_tasks_pending_)
    return;

  check_for_completed_tasks_callback_.Reset(
      base::Bind(&WorkerPool::CheckForCompletedTasks,
                 weak_ptr_factory_.GetWeakPtr()));
  origin_loop_->PostDelayedTask(
      FROM_HERE,
      check_for_completed_tasks_callback_.callback(),
      base::TimeDelta::FromMilliseconds(kCheckForCompletedTasksDelayMs));
  check_for_completed_tasks_pending_ = true;
}

void WorkerPool::WillPostTask() {
  base::subtle::Barrier_AtomicIncrement(&pending_task_count_, 1);
  ScheduleCheckForCompletedTasks();
  workers_need_sorting_ = true;
}

void WorkerPool::OnWorkCompletedOnWorkerThread() {
  // Post idle handler task when pool work count reaches 0.
  if (base::subtle::Barrier_AtomicIncrement(&pending_task_count_, -1) == 0) {
    origin_loop_->PostTask(FROM_HERE, idle_callback_);
  }
}

void WorkerPool::OnIdle() {
  if (base::subtle::Acquire_Load(&pending_task_count_) == 0) {
    check_for_completed_tasks_callback_.Cancel();
    CheckForCompletedTasks();
  }
}

void WorkerPool::CheckForCompletedTasks() {
  check_for_completed_tasks_pending_ = false;

  for (WorkerVector::iterator it = workers_.begin();
       it != workers_.end(); it++) {
    Worker* worker = *it;
    worker->CheckForCompletedTasks();
  }

  client_->DidFinishDispatchingWorkerPoolCompletionCallbacks();

  for (WorkerVector::iterator it = workers_.begin();
       it != workers_.end(); it++) {
    Worker* worker = *it;
    if (worker->num_pending_tasks()) {
      ScheduleCheckForCompletedTasks();
      break;
    }
  }
}

void WorkerPool::OnTaskCompleted() {
  workers_need_sorting_ = true;
}

void WorkerPool::SortWorkersIfNeeded() {
  if (!workers_need_sorting_)
    return;

  std::sort(workers_.begin(), workers_.end(), NumPendingTasksComparator());
  workers_need_sorting_ = false;
}

}  // namespace cc
