// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/task_queue_manager.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "content/renderer/scheduler/task_queue_selector.h"

namespace content {
namespace internal {

class TaskQueue : public base::SingleThreadTaskRunner {
 public:
  TaskQueue(TaskQueueManager* task_queue_manager);

  // base::SingleThreadTaskRunner implementation.
  bool RunsTasksOnCurrentThread() const override;
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override;

  // Adds a task at the end of the incoming task queue and schedules a call to
  // TaskQueueManager::DoWork() if the incoming queue was empty and automatic
  // pumping is enabled. Can be called on an arbitrary thread.
  void EnqueueTask(const base::PendingTask& pending_task);

  bool IsQueueEmpty() const;

  void SetAutoPump(bool auto_pump);
  void PumpQueue();

  bool UpdateWorkQueue();
  base::PendingTask TakeTaskFromWorkQueue();

  void WillDeleteTaskQueueManager();

  base::TaskQueue& work_queue() { return work_queue_; }

 private:
  ~TaskQueue() override;

  void PumpQueueLocked();
  void EnqueueTaskLocked(const base::PendingTask& pending_task);

  // This lock protects all members except the work queue.
  mutable base::Lock lock_;
  TaskQueueManager* task_queue_manager_;
  base::TaskQueue incoming_queue_;
  bool auto_pump_;

  base::TaskQueue work_queue_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueue);
};

TaskQueue::TaskQueue(TaskQueueManager* task_queue_manager)
    : task_queue_manager_(task_queue_manager), auto_pump_(true) {
}

TaskQueue::~TaskQueue() {
}

void TaskQueue::WillDeleteTaskQueueManager() {
  base::AutoLock lock(lock_);
  task_queue_manager_ = nullptr;
}

bool TaskQueue::RunsTasksOnCurrentThread() const {
  base::AutoLock lock(lock_);
  if (!task_queue_manager_)
    return false;
  return task_queue_manager_->RunsTasksOnCurrentThread();
}

bool TaskQueue::PostDelayedTask(const tracked_objects::Location& from_here,
                                const base::Closure& task,
                                base::TimeDelta delay) {
  base::AutoLock lock(lock_);
  if (!task_queue_manager_)
    return false;

  base::PendingTask pending_task(from_here, task);
  task_queue_manager_->DidQueueTask(&pending_task);

  if (delay > base::TimeDelta()) {
    return task_queue_manager_->PostDelayedTask(
        from_here, Bind(&TaskQueue::EnqueueTask, this, pending_task), delay);
  }
  EnqueueTaskLocked(pending_task);
  return true;
}

bool TaskQueue::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  base::AutoLock lock(lock_);
  if (!task_queue_manager_)
    return false;
  return task_queue_manager_->PostNonNestableDelayedTask(
      from_here, task, delay);
}

bool TaskQueue::IsQueueEmpty() const {
  if (!work_queue_.empty())
    return false;

  {
    base::AutoLock lock(lock_);
    return incoming_queue_.empty();
  }
}

bool TaskQueue::UpdateWorkQueue() {
  if (!work_queue_.empty())
    return true;

  {
    base::AutoLock lock(lock_);
    if (!auto_pump_ || incoming_queue_.empty())
      return false;
    work_queue_.Swap(&incoming_queue_);
    return true;
  }
}

base::PendingTask TaskQueue::TakeTaskFromWorkQueue() {
  base::PendingTask pending_task = work_queue_.front();
  work_queue_.pop();
  return pending_task;
}

void TaskQueue::EnqueueTask(const base::PendingTask& pending_task) {
  base::AutoLock lock(lock_);
  EnqueueTaskLocked(pending_task);
}

void TaskQueue::EnqueueTaskLocked(const base::PendingTask& pending_task) {
  lock_.AssertAcquired();
  if (!task_queue_manager_)
    return;
  if (auto_pump_ && incoming_queue_.empty())
    task_queue_manager_->PostDoWorkOnMainRunner();
  incoming_queue_.push(pending_task);
}

void TaskQueue::SetAutoPump(bool auto_pump) {
  base::AutoLock lock(lock_);
  if (auto_pump) {
    auto_pump_ = true;
    PumpQueueLocked();
  } else {
    auto_pump_ = false;
  }
}

void TaskQueue::PumpQueueLocked() {
  lock_.AssertAcquired();
  while (!incoming_queue_.empty()) {
    work_queue_.push(incoming_queue_.front());
    incoming_queue_.pop();
  }
  if (!work_queue_.empty())
    task_queue_manager_->PostDoWorkOnMainRunner();
}

void TaskQueue::PumpQueue() {
  base::AutoLock lock(lock_);
  PumpQueueLocked();
}

}  // namespace

TaskQueueManager::TaskQueueManager(
    size_t task_queue_count,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    TaskQueueSelector* selector)
    : main_task_runner_(main_task_runner),
      selector_(selector),
      weak_factory_(this) {
  DCHECK(main_task_runner->RunsTasksOnCurrentThread());

  task_queue_manager_weak_ptr_ = weak_factory_.GetWeakPtr();
  for (size_t i = 0; i < task_queue_count; i++) {
    scoped_refptr<internal::TaskQueue> queue(
        make_scoped_refptr(new internal::TaskQueue(this)));
    queues_.push_back(queue);
  }

  std::vector<const base::TaskQueue*> work_queues;
  for (const auto& queue: queues_)
    work_queues.push_back(&queue->work_queue());
  selector_->RegisterWorkQueues(work_queues);
}

TaskQueueManager::~TaskQueueManager() {
  for (auto& queue : queues_)
    queue->WillDeleteTaskQueueManager();
}

internal::TaskQueue* TaskQueueManager::Queue(size_t queue_index) const {
  DCHECK_LT(queue_index, queues_.size());
  return queues_[queue_index].get();
}

scoped_refptr<base::SingleThreadTaskRunner>
TaskQueueManager::TaskRunnerForQueue(size_t queue_index) const {
  return Queue(queue_index);
}

bool TaskQueueManager::IsQueueEmpty(size_t queue_index) const {
  internal::TaskQueue* queue = Queue(queue_index);
  return queue->IsQueueEmpty();
}

void TaskQueueManager::SetAutoPump(size_t queue_index, bool auto_pump) {
  main_thread_checker_.CalledOnValidThread();
  internal::TaskQueue* queue = Queue(queue_index);
  queue->SetAutoPump(auto_pump);
}

void TaskQueueManager::PumpQueue(size_t queue_index) {
  main_thread_checker_.CalledOnValidThread();
  internal::TaskQueue* queue = Queue(queue_index);
  queue->PumpQueue();
}

bool TaskQueueManager::UpdateWorkQueues() {
  // TODO(skyostil): This is not efficient when the number of queues grows very
  // large due to the number of locks taken. Consider optimizing when we get
  // there.
  main_thread_checker_.CalledOnValidThread();
  bool has_work = false;
  for (auto& queue : queues_)
    has_work |= queue->UpdateWorkQueue();
  return has_work;
}

void TaskQueueManager::PostDoWorkOnMainRunner() {
  main_task_runner_->PostTask(
      FROM_HERE, Bind(&TaskQueueManager::DoWork, task_queue_manager_weak_ptr_));
}

void TaskQueueManager::DoWork() {
  main_thread_checker_.CalledOnValidThread();
  if (!UpdateWorkQueues())
    return;

  size_t queue_index;
  if (!selector_->SelectWorkQueueToService(&queue_index))
    return;
  PostDoWorkOnMainRunner();
  RunTaskFromWorkQueue(queue_index);
}

void TaskQueueManager::DidQueueTask(base::PendingTask* pending_task) {
  pending_task->sequence_num = task_sequence_num_.GetNext();
  task_annotator_.DidQueueTask("TaskQueueManager::PostTask", *pending_task);
}

void TaskQueueManager::RunTaskFromWorkQueue(size_t queue_index) {
  main_thread_checker_.CalledOnValidThread();
  internal::TaskQueue* queue = Queue(queue_index);
  base::PendingTask pending_task = queue->TakeTaskFromWorkQueue();
  task_annotator_.RunTask(
      "TaskQueueManager::PostTask", "TaskQueueManager::RunTask", pending_task);
}

bool TaskQueueManager::RunsTasksOnCurrentThread() const {
  return main_task_runner_->RunsTasksOnCurrentThread();
}

bool TaskQueueManager::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  DCHECK(delay > base::TimeDelta());
  return main_task_runner_->PostDelayedTask(from_here, task, delay);
}

bool TaskQueueManager::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  // Defer non-nestable work to the main task runner.
  return main_task_runner_->PostNonNestableDelayedTask(from_here, task, delay);
}

}  // namespace content
