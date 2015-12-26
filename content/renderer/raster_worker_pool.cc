// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/raster_worker_pool.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"

namespace content {

// A sequenced task runner which posts tasks to a RasterWorkerPool.
class RasterWorkerPool::RasterWorkerPoolSequencedTaskRunner
    : public base::SequencedTaskRunner {
 public:
  explicit RasterWorkerPoolSequencedTaskRunner(
      cc::TaskGraphRunner* task_graph_runner)
      : task_graph_runner_(task_graph_runner),
        namespace_token_(task_graph_runner->GetNamespaceToken()) {}

  // Overridden from base::TaskRunner:
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override {
    return PostNonNestableDelayedTask(from_here, task, delay);
  }
  bool RunsTasksOnCurrentThread() const override { return true; }

  // Overridden from base::SequencedTaskRunner:
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override {
    base::AutoLock lock(lock_);

    // Remove completed tasks.
    DCHECK(completed_tasks_.empty());
    task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                              &completed_tasks_);

    tasks_.erase(tasks_.begin(), tasks_.begin() + completed_tasks_.size());

    tasks_.push_back(make_scoped_refptr(new ClosureTask(task)));
    graph_.Reset();
    for (const auto& graph_task : tasks_) {
      int dependencies = 0;
      if (!graph_.nodes.empty())
        dependencies = 1;

      cc::TaskGraph::Node node(graph_task.get(), 0u /* category */,
                               0u /* priority */, dependencies);
      if (dependencies) {
        graph_.edges.push_back(
            cc::TaskGraph::Edge(graph_.nodes.back().task, node.task));
      }
      graph_.nodes.push_back(node);
    }
    task_graph_runner_->ScheduleTasks(namespace_token_, &graph_);
    completed_tasks_.clear();
    return true;
  }

 private:
  ~RasterWorkerPoolSequencedTaskRunner() override {
    task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
    task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                              &completed_tasks_);
  };

  // Lock to exclusively access all the following members that are used to
  // implement the SequencedTaskRunner interfaces.
  base::Lock lock_;

  cc::TaskGraphRunner* task_graph_runner_;
  // Namespace used to schedule tasks in the task graph runner.
  cc::NamespaceToken namespace_token_;
  // List of tasks currently queued up for execution.
  cc::Task::Vector tasks_;
  // Graph object used for scheduling tasks.
  cc::TaskGraph graph_;
  // Cached vector to avoid allocation when getting the list of complete
  // tasks.
  cc::Task::Vector completed_tasks_;
};

RasterWorkerPool::RasterWorkerPool()
    : namespace_token_(GetNamespaceToken()),
      has_ready_to_run_tasks_cv_(&lock_),
      has_namespaces_with_finished_running_tasks_cv_(&lock_),
      shutdown_(false) {}

void RasterWorkerPool::Start(
    int num_threads,
    const base::SimpleThread::Options& thread_options) {
  DCHECK(threads_.empty());
  while (threads_.size() < static_cast<size_t>(num_threads)) {
    scoped_ptr<base::DelegateSimpleThread> thread(
        new base::DelegateSimpleThread(
            this, base::StringPrintf("CompositorTileWorker%u",
                                     static_cast<unsigned>(threads_.size() + 1))
                      .c_str(),
            thread_options));
    thread->Start();
    threads_.push_back(std::move(thread));
  }
}

void RasterWorkerPool::Shutdown() {
  WaitForTasksToFinishRunning(namespace_token_);
  CollectCompletedTasks(namespace_token_, &completed_tasks_);
  // Shutdown raster threads.
  {
    base::AutoLock lock(lock_);

    DCHECK(!work_queue_.HasReadyToRunTasks());
    DCHECK(!work_queue_.HasAnyNamespaces());

    DCHECK(!shutdown_);
    shutdown_ = true;

    // Wake up all workers so they exit.
    has_ready_to_run_tasks_cv_.Broadcast();
  }
  while (!threads_.empty()) {
    threads_.back()->Join();
    threads_.pop_back();
  }
}

// Overridden from base::TaskRunner:
bool RasterWorkerPool::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  base::AutoLock lock(lock_);

  // Remove completed tasks.
  DCHECK(completed_tasks_.empty());
  CollectCompletedTasksWithLockAcquired(namespace_token_, &completed_tasks_);

  cc::Task::Vector::iterator end = std::remove_if(
      tasks_.begin(), tasks_.end(), [this](const scoped_refptr<cc::Task>& e) {
        return std::find(this->completed_tasks_.begin(),
                         this->completed_tasks_.end(),
                         e) != this->completed_tasks_.end();
      });
  tasks_.erase(end, tasks_.end());

  tasks_.push_back(make_scoped_refptr(new ClosureTask(task)));
  graph_.Reset();
  for (const auto& graph_task : tasks_)
    graph_.nodes.push_back(
        cc::TaskGraph::Node(graph_task.get(), 0u /* category */,
                            0u /* priority */, 0u /* dependencies */));

  ScheduleTasksWithLockAcquired(namespace_token_, &graph_);
  completed_tasks_.clear();
  return true;
}

bool RasterWorkerPool::RunsTasksOnCurrentThread() const {
  return true;
}

// Overridden from base::DelegateSimpleThread::Delegate:
void RasterWorkerPool::Run() {
  base::AutoLock lock(lock_);

  while (true) {
    if (!RunTaskWithLockAcquired()) {
      // Exit when shutdown is set and no more tasks are pending.
      if (shutdown_)
        break;

      // Wait for more tasks.
      has_ready_to_run_tasks_cv_.Wait();
      continue;
    }
  }
}

void RasterWorkerPool::FlushForTesting() {
  base::AutoLock lock(lock_);

  while (!work_queue_.HasFinishedRunningTasksInAllNamespaces()) {
    has_namespaces_with_finished_running_tasks_cv_.Wait();
  }
}

scoped_refptr<base::SequencedTaskRunner>
RasterWorkerPool::CreateSequencedTaskRunner() {
  return new RasterWorkerPoolSequencedTaskRunner(this);
}

RasterWorkerPool::~RasterWorkerPool() {}

cc::NamespaceToken RasterWorkerPool::GetNamespaceToken() {
  base::AutoLock lock(lock_);
  return work_queue_.GetNamespaceToken();
}

void RasterWorkerPool::ScheduleTasks(cc::NamespaceToken token,
                                     cc::TaskGraph* graph) {
  TRACE_EVENT2("cc", "RasterWorkerPool::ScheduleTasks", "num_nodes",
               graph->nodes.size(), "num_edges", graph->edges.size());
  {
    base::AutoLock lock(lock_);
    ScheduleTasksWithLockAcquired(token, graph);
  }
}

void RasterWorkerPool::ScheduleTasksWithLockAcquired(cc::NamespaceToken token,
                                                     cc::TaskGraph* graph) {
  DCHECK(token.IsValid());
  DCHECK(!cc::TaskGraphWorkQueue::DependencyMismatch(graph));
  DCHECK(!shutdown_);

  work_queue_.ScheduleTasks(token, graph);

  // If there is more work available, wake up worker thread.
  if (work_queue_.HasReadyToRunTasks())
    has_ready_to_run_tasks_cv_.Signal();
}

void RasterWorkerPool::WaitForTasksToFinishRunning(cc::NamespaceToken token) {
  TRACE_EVENT0("cc", "RasterWorkerPool::WaitForTasksToFinishRunning");

  DCHECK(token.IsValid());

  {
    base::AutoLock lock(lock_);
    base::ThreadRestrictions::ScopedAllowWait allow_wait;

    auto* task_namespace = work_queue_.GetNamespaceForToken(token);

    if (!task_namespace)
      return;

    while (!work_queue_.HasFinishedRunningTasksInNamespace(task_namespace))
      has_namespaces_with_finished_running_tasks_cv_.Wait();
  }
}

void RasterWorkerPool::CollectCompletedTasks(
    cc::NamespaceToken token,
    cc::Task::Vector* completed_tasks) {
  TRACE_EVENT0("cc", "RasterWorkerPool::CollectCompletedTasks");

  {
    base::AutoLock lock(lock_);
    CollectCompletedTasksWithLockAcquired(token, completed_tasks);
  }
}

void RasterWorkerPool::CollectCompletedTasksWithLockAcquired(
    cc::NamespaceToken token,
    cc::Task::Vector* completed_tasks) {
  DCHECK(token.IsValid());
  work_queue_.CollectCompletedTasks(token, completed_tasks);
}

bool RasterWorkerPool::RunTaskWithLockAcquired() {
  TRACE_EVENT0("toplevel", "TaskGraphRunner::RunTask");

  lock_.AssertAcquired();

  // Find the first category with any tasks to run. This task graph runner
  // treats categories as an additional priority.
  // TODO(ericrk): Add more category/thread logic.
  const auto& ready_to_run_namespaces = work_queue_.ready_to_run_namespaces();
  auto found = std::find_if(
      ready_to_run_namespaces.cbegin(), ready_to_run_namespaces.cend(),
      [](const std::pair<uint16_t,
                         cc::TaskGraphWorkQueue::TaskNamespace::Vector>& pair) {
        return !pair.second.empty();
      });

  if (found == ready_to_run_namespaces.cend()) {
    return false;
  }

  const uint16_t category = found->first;
  auto prioritized_task = work_queue_.GetNextTaskToRun(category);
  cc::Task* task = prioritized_task.task;

  // There may be more work available, so wake up another worker thread.
  if (work_queue_.HasReadyToRunTasks())
    has_ready_to_run_tasks_cv_.Signal();

  // Call WillRun() before releasing |lock_| and running task.
  task->WillRun();

  {
    base::AutoUnlock unlock(lock_);

    task->RunOnWorkerThread();
  }

  // This will mark task as finished running.
  task->DidRun();

  work_queue_.CompleteTask(prioritized_task);

  // If namespace has finished running all tasks, wake up origin threads.
  if (work_queue_.HasFinishedRunningTasksInNamespace(
          prioritized_task.task_namespace))
    has_namespaces_with_finished_running_tasks_cv_.Broadcast();

  return true;
}

RasterWorkerPool::ClosureTask::ClosureTask(const base::Closure& closure)
    : closure_(closure) {}

// Overridden from cc::Task:
void RasterWorkerPool::ClosureTask::RunOnWorkerThread() {
  closure_.Run();
  closure_.Reset();
}

RasterWorkerPool::ClosureTask::~ClosureTask() {}

}  // namespace content
