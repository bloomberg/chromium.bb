// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/task_queue_manager.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "content/renderer/scheduler/task_queue_selector.h"

namespace content {
namespace internal {

class TaskRunner : public base::SingleThreadTaskRunner {
 public:
  TaskRunner(base::WeakPtr<TaskQueueManager> task_queue_manager,
             size_t queue_index);

  // base::SingleThreadTaskRunner implementation.
  virtual bool RunsTasksOnCurrentThread() const override;
  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               base::TimeDelta delay) override;
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) override;

 private:
  virtual ~TaskRunner();

  base::WeakPtr<TaskQueueManager> task_queue_manager_;
  const size_t queue_index_;

  DISALLOW_COPY_AND_ASSIGN(TaskRunner);
};

TaskRunner::TaskRunner(base::WeakPtr<TaskQueueManager> task_queue_manager,
                       size_t queue_index)
    : task_queue_manager_(task_queue_manager), queue_index_(queue_index) {
}

TaskRunner::~TaskRunner() {
}

bool TaskRunner::RunsTasksOnCurrentThread() const {
  if (!task_queue_manager_)
    return false;
  return task_queue_manager_->RunsTasksOnCurrentThread();
}

bool TaskRunner::PostDelayedTask(const tracked_objects::Location& from_here,
                                 const base::Closure& task,
                                 base::TimeDelta delay) {
  if (!task_queue_manager_)
    return false;
  return task_queue_manager_->PostDelayedTask(
      queue_index_, from_here, task, delay);
}

bool TaskRunner::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  if (!task_queue_manager_)
    return false;
  return task_queue_manager_->PostNonNestableDelayedTask(
      queue_index_, from_here, task, delay);
}

struct TaskQueue {
  TaskQueue() : auto_pump(true) {}
  ~TaskQueue() {}

  scoped_refptr<TaskRunner> task_runner;

  base::Lock incoming_queue_lock;
  base::TaskQueue incoming_queue;

  bool auto_pump;
  base::TaskQueue work_queue;

  DISALLOW_COPY_AND_ASSIGN(TaskQueue);
};

}  // namespace

TaskQueueManager::TaskQueueManager(
    size_t task_queue_count,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    TaskQueueSelector* selector)
    : main_task_runner_(main_task_runner),
      selector_(selector),
      weak_factory_(this) {
  DCHECK(main_task_runner->RunsTasksOnCurrentThread());

  for (size_t i = 0; i < task_queue_count; i++) {
    scoped_ptr<internal::TaskQueue> queue(new internal::TaskQueue());
    queue->task_runner = make_scoped_refptr(
        new internal::TaskRunner(weak_factory_.GetWeakPtr(), i));
    queues_.push_back(queue.release());
  }

  std::vector<const base::TaskQueue*> work_queues;
  for (const auto& queue: queues_)
    work_queues.push_back(&queue->work_queue);
  selector_->RegisterWorkQueues(work_queues);
}

TaskQueueManager::~TaskQueueManager() {
}

internal::TaskQueue* TaskQueueManager::Queue(size_t queue_index) const {
  DCHECK_LT(queue_index, queues_.size());
  return queues_[queue_index];
}

scoped_refptr<base::SingleThreadTaskRunner>
TaskQueueManager::TaskRunnerForQueue(size_t queue_index) const {
  return Queue(queue_index)->task_runner;
}

bool TaskQueueManager::IsQueueEmpty(size_t queue_index) {
  internal::TaskQueue* queue = Queue(queue_index);
  if (!queue->work_queue.empty())
    return false;
  base::AutoLock lock(queue->incoming_queue_lock);
  return queue->incoming_queue.empty();
}

void TaskQueueManager::EnqueueTask(size_t queue_index,
                                   const base::PendingTask& pending_task) {
  internal::TaskQueue* queue = Queue(queue_index);
  base::AutoLock lock(queue->incoming_queue_lock);
  if (queue->auto_pump && queue->incoming_queue.empty())
    PostDoWorkOnMainRunner();
  queue->incoming_queue.push(pending_task);
}

void TaskQueueManager::SetAutoPump(size_t queue_index, bool auto_pump) {
  internal::TaskQueue* queue = Queue(queue_index);
  base::AutoLock lock(queue->incoming_queue_lock);
  if (auto_pump) {
    queue->auto_pump = true;
    PumpQueueLocked(queue);
  } else {
    queue->auto_pump = false;
  }
}

void TaskQueueManager::PumpQueueLocked(internal::TaskQueue* queue) {
  main_thread_checker_.CalledOnValidThread();
  queue->incoming_queue_lock.AssertAcquired();
  while (!queue->incoming_queue.empty()) {
    queue->work_queue.push(queue->incoming_queue.front());
    queue->incoming_queue.pop();
  }
  if (!queue->work_queue.empty())
    PostDoWorkOnMainRunner();
}

void TaskQueueManager::PumpQueue(size_t queue_index) {
  main_thread_checker_.CalledOnValidThread();
  internal::TaskQueue* queue = Queue(queue_index);
  base::AutoLock lock(queue->incoming_queue_lock);
  PumpQueueLocked(queue);
}

bool TaskQueueManager::UpdateWorkQueues() {
  // TODO(skyostil): This is not efficient when the number of queues grows very
  // large due to the number of locks taken. Consider optimizing when we get
  // there.
  main_thread_checker_.CalledOnValidThread();
  bool has_work = false;
  for (auto& queue: queues_) {
    if (!queue->work_queue.empty()) {
      has_work = true;
      continue;
    }
    base::AutoLock lock(queue->incoming_queue_lock);
    if (!queue->auto_pump || queue->incoming_queue.empty())
      continue;
    queue->work_queue.Swap(&queue->incoming_queue);
    has_work = true;
  }
  return has_work;
}

void TaskQueueManager::PostDoWorkOnMainRunner() {
  main_task_runner_->PostTask(
      FROM_HERE, Bind(&TaskQueueManager::DoWork, weak_factory_.GetWeakPtr()));
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

void TaskQueueManager::RunTaskFromWorkQueue(size_t queue_index) {
  main_thread_checker_.CalledOnValidThread();
  internal::TaskQueue* queue = Queue(queue_index);
  DCHECK(!queue->work_queue.empty());
  base::PendingTask pending_task = queue->work_queue.front();
  queue->work_queue.pop();
  task_annotator_.RunTask(
      "TaskQueueManager::PostTask", "TaskQueueManager::RunTask", pending_task);
}

bool TaskQueueManager::RunsTasksOnCurrentThread() const {
  return main_task_runner_->RunsTasksOnCurrentThread();
}

bool TaskQueueManager::PostDelayedTask(
    size_t queue_index,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  int sequence_num = task_sequence_num_.GetNext();

  base::PendingTask pending_task(from_here, task);
  pending_task.sequence_num = sequence_num;

  task_annotator_.DidQueueTask("TaskQueueManager::PostTask", pending_task);
  if (delay > base::TimeDelta()) {
    return main_task_runner_->PostDelayedTask(
        from_here,
        Bind(&TaskQueueManager::EnqueueTask,
             weak_factory_.GetWeakPtr(),
             queue_index,
             pending_task),
        delay);
  }
  EnqueueTask(queue_index, pending_task);
  return true;
}

bool TaskQueueManager::PostNonNestableDelayedTask(
    size_t queue_index,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  // Defer non-nestable work to the main task runner.
  return main_task_runner_->PostNonNestableDelayedTask(from_here, task, delay);
}

}  // namespace content
