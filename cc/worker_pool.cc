// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/worker_pool.h"

#include <algorithm>

#include "base/bind.h"
#include "base/debug/trace_event.h"
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

  virtual void WillRunOnThread(base::Thread* thread) OVERRIDE {}

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

// Limits for the total number of cheap tasks we are allowed to perform
// during a single frame and the time spent running those tasks.
// TODO(skyostil): Determine these limits more dynamically.
const int kMaxCheapTaskCount = 6;
const int kMaxCheapTaskMs = kCheckForCompletedTasksDelayMs;

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
      idle_callback_(
          base::Bind(&WorkerPool::OnIdle, weak_ptr_factory_.GetWeakPtr())),
      cheap_task_callback_(
          base::Bind(&WorkerPool::RunCheapTasks,
                     weak_ptr_factory_.GetWeakPtr())),
      run_cheap_tasks_pending_(false) {
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

  if (run_cheap_tasks_pending_)
    RunCheapTasks();

  for (WorkerVector::iterator it = workers_.begin();
       it != workers_.end(); it++) {
    Worker* worker = *it;
    worker->StopAfterCompletingAllPendingTasks();
  }
}

void WorkerPool::PostTaskAndReply(
    const Callback& task, const base::Closure& reply) {
  PostTask(
      make_scoped_ptr(new WorkerPoolTaskImpl(
                          task,
                          reply)).PassAs<internal::WorkerPoolTask>(),
                      false);
}

bool WorkerPool::IsBusy() {
  Worker* worker = GetWorkerForNextTask();

  return worker->num_pending_tasks() >= kNumPendingTasksPerWorker;
}

void WorkerPool::SetRecordRenderingStats(bool record_rendering_stats) {
  if (record_rendering_stats)
    cheap_rendering_stats_.reset(new RenderingStats);
  else
    cheap_rendering_stats_.reset();

  for (WorkerVector::iterator it = workers_.begin();
       it != workers_.end(); ++it) {
    Worker* worker = *it;
    worker->set_record_rendering_stats(record_rendering_stats);
  }
}

void WorkerPool::GetRenderingStats(RenderingStats* stats) {
  stats->totalRasterizeTime = base::TimeDelta();
  stats->totalRasterizeTimeForNowBinsOnPendingTree = base::TimeDelta();
  stats->totalPixelsRasterized = 0;
  stats->totalDeferredImageDecodeCount = 0;
  stats->totalDeferredImageDecodeTime = base::TimeDelta();
  if (cheap_rendering_stats_) {
    stats->totalRasterizeTime +=
        cheap_rendering_stats_->totalRasterizeTime;
    stats->totalPixelsRasterized +=
        cheap_rendering_stats_->totalPixelsRasterized;
    stats->totalDeferredImageDecodeCount +=
        cheap_rendering_stats_->totalDeferredImageDecodeCount;
    stats->totalDeferredImageDecodeTime +=
        cheap_rendering_stats_->totalDeferredImageDecodeTime;
  }
  for (WorkerVector::iterator it = workers_.begin();
       it != workers_.end(); ++it) {
    Worker* worker = *it;
    CHECK(worker->rendering_stats());
    stats->totalRasterizeTime +=
        worker->rendering_stats()->totalRasterizeTime;
    stats->totalRasterizeTimeForNowBinsOnPendingTree +=
        worker->rendering_stats()->totalRasterizeTimeForNowBinsOnPendingTree;
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
  if (!check_for_completed_tasks_deadline_.is_null())
    return;

  check_for_completed_tasks_callback_.Reset(
      base::Bind(&WorkerPool::CheckForCompletedTasks,
                 weak_ptr_factory_.GetWeakPtr()));
  base::TimeDelta delay =
      base::TimeDelta::FromMilliseconds(kCheckForCompletedTasksDelayMs);
  check_for_completed_tasks_deadline_ = base::TimeTicks::Now() + delay;
  origin_loop_->PostDelayedTask(
      FROM_HERE,
      check_for_completed_tasks_callback_.callback(),
      delay);
}

void WorkerPool::OnWorkCompletedOnWorkerThread() {
  // Post idle handler task when pool work count reaches 0.
  if (base::subtle::Barrier_AtomicIncrement(&pending_task_count_, -1) == 0) {
    origin_loop_->PostTask(FROM_HERE, idle_callback_);
  }
}

void WorkerPool::OnIdle() {
  if (base::subtle::Acquire_Load(&pending_task_count_) == 0 &&
      pending_cheap_tasks_.empty()) {
    check_for_completed_tasks_callback_.Cancel();
    CheckForCompletedTasks();
  }
}

void WorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "WorkerPool::CheckForCompletedTasks");
  check_for_completed_tasks_deadline_ = base::TimeTicks();

  while (completed_cheap_tasks_.size()) {
    scoped_ptr<internal::WorkerPoolTask> task =
        completed_cheap_tasks_.take_front();
    task->DidComplete();
  }

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

void WorkerPool::PostTask(
    scoped_ptr<internal::WorkerPoolTask> task, bool is_cheap) {
  if (is_cheap && CanPostCheapTask()) {
    pending_cheap_tasks_.push_back(task.Pass());
    ScheduleRunCheapTasks();
  } else {
    base::subtle::Barrier_AtomicIncrement(&pending_task_count_, 1);
    workers_need_sorting_ = true;

    Worker* worker = GetWorkerForNextTask();
    task->WillRunOnThread(worker);
    worker->PostTask(task.Pass());
  }
  ScheduleCheckForCompletedTasks();
}

bool WorkerPool::CanPostCheapTask() const {
  return pending_cheap_tasks_.size() < kMaxCheapTaskCount;
}

void WorkerPool::ScheduleRunCheapTasks() {
  if (run_cheap_tasks_pending_)
    return;
  origin_loop_->PostTask(FROM_HERE, cheap_task_callback_);
  run_cheap_tasks_pending_ = true;
}

void WorkerPool::RunCheapTasks() {
  TRACE_EVENT0("cc", "WorkerPool::RunCheapTasks");
  DCHECK(run_cheap_tasks_pending_);

  // Run as many cheap tasks as we can within the time limit.
  base::TimeTicks deadline = base::TimeTicks::Now() +
      base::TimeDelta::FromMilliseconds(kMaxCheapTaskMs);
  while (pending_cheap_tasks_.size()) {
    scoped_ptr<internal::WorkerPoolTask> task =
        pending_cheap_tasks_.take_front();
    task->Run(cheap_rendering_stats_.get());
    completed_cheap_tasks_.push_back(task.Pass());

    if (!check_for_completed_tasks_deadline_.is_null() &&
        base::TimeTicks::Now() >= check_for_completed_tasks_deadline_) {
      TRACE_EVENT_INSTANT0("cc", "WorkerPool::RunCheapTasks check deadline");
      CheckForCompletedTasks();
    }
    if (base::TimeTicks::Now() >= deadline) {
      TRACE_EVENT_INSTANT0("cc", "WorkerPool::RunCheapTasks out of time");
      break;
    }
  }

  // Defer remaining tasks to worker threads.
  while (pending_cheap_tasks_.size()) {
    scoped_ptr<internal::WorkerPoolTask> task =
        pending_cheap_tasks_.take_front();
    PostTask(task.Pass(), false);
  }
  run_cheap_tasks_pending_ = false;
}

}  // namespace cc
