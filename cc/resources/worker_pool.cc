// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/worker_pool.h"

#include <algorithm>

#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/condition_variable.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_restrictions.h"
#include "cc/base/scoped_ptr_deque.h"

namespace cc {

namespace {

// TaskGraphRunners can process task graphs from multiple
// workerpool instances. All members are guarded by |lock_|.
class TaskGraphRunner : public base::DelegateSimpleThread::Delegate {
 public:
  typedef WorkerPool::TaskGraph TaskGraph;
  typedef WorkerPool::TaskVector TaskVector;

  TaskGraphRunner(size_t num_threads, const std::string& thread_name_prefix);
  virtual ~TaskGraphRunner();

  void Register(const WorkerPool* worker_pool);
  void Unregister(const WorkerPool* worker_pool);
  // Schedule running of tasks in |graph|. Tasks previously scheduled but
  // no longer needed will be canceled unless already running. Canceled
  // tasks are moved to |completed_tasks| without being run. The result
  // is that once scheduled, a task is guaranteed to end up in the
  // |completed_tasks| queue even if it later get canceled by another
  // call to SetTaskGraph().
  void SetTaskGraph(const WorkerPool* worker_pool, TaskGraph* graph);

  // Wait for all scheduled tasks to finish running.
  void WaitForTasksToFinishRunning(const WorkerPool* worker_pool);

  // Collect all completed tasks in |completed_tasks|.
  void CollectCompletedTasks(const WorkerPool* worker_pool,
                             TaskVector* completed_tasks);

 private:
  static bool CompareTaskPriority(const internal::GraphNode* a,
                                  const internal::GraphNode* b) {
    // In this system, numerically lower priority is run first.
    if (a->priority() != b->priority())
      return a->priority() > b->priority();

    // Run task with most dependents first when priority is the same.
    return a->dependents().size() < b->dependents().size();
  }

  struct TaskNamespace {
    // This set contains all pending tasks.
    TaskGraph pending_tasks;
    // This set contains all currently running tasks.
    TaskGraph running_tasks;
    // Completed tasks not yet collected by origin thread.
    TaskVector completed_tasks;
    // Ordered set of tasks that are ready to run.
    internal::GraphNode::Vector ready_to_run_tasks;
  };

  static bool CompareTaskNamespacePriority(const TaskNamespace* a,
                                           const TaskNamespace* b) {
    DCHECK(!a->ready_to_run_tasks.empty());
    DCHECK(!b->ready_to_run_tasks.empty());

    // Compare based on task priority of the ready_to_run_tasks heap
    // .front() will hold the max element of the heap,
    // except after pop_heap, when max element is moved to .back().
    return CompareTaskPriority(a->ready_to_run_tasks.front(),
                               b->ready_to_run_tasks.front());
  }

  typedef std::map<const WorkerPool*, linked_ptr<TaskNamespace> >
      TaskNamespaceMap;

  // Overridden from base::DelegateSimpleThread:
  virtual void Run() OVERRIDE;

  inline bool has_finished_running_tasks(TaskNamespace* task_namespace) {
    return (task_namespace->pending_tasks.empty() &&
            task_namespace->running_tasks.empty());
  }

  // This lock protects all members of this class except
  // |worker_pool_on_origin_thread_|. Do not read or modify anything
  // without holding this lock. Do not block while holding this lock.
  mutable base::Lock lock_;

  // Condition variable that is waited on by worker threads until new
  // tasks are ready to run or shutdown starts.
  base::ConditionVariable has_ready_to_run_tasks_cv_;

  // Condition variable that is waited on by origin threads until a
  // namespace has finished running all associated tasks.
  base::ConditionVariable has_namespaces_with_finished_running_tasks_cv_;

  // Provides each running thread loop with a unique index. First thread
  // loop index is 0.
  unsigned next_thread_index_;

  // Set during shutdown. Tells workers to exit when no more tasks
  // are pending.
  bool shutdown_;

  // This set contains all registered namespaces.
  TaskNamespaceMap namespaces_;

  // Ordered set of task namespaces that have ready to run tasks.
  std::vector<TaskNamespace*> ready_to_run_namespaces_;

  ScopedPtrDeque<base::DelegateSimpleThread> workers_;

  DISALLOW_COPY_AND_ASSIGN(TaskGraphRunner);
};

TaskGraphRunner::TaskGraphRunner(
    size_t num_threads, const std::string& thread_name_prefix)
    : lock_(),
      has_ready_to_run_tasks_cv_(&lock_),
      has_namespaces_with_finished_running_tasks_cv_(&lock_),
      next_thread_index_(0),
      shutdown_(false) {
  base::AutoLock lock(lock_);

  while (workers_.size() < num_threads) {
    scoped_ptr<base::DelegateSimpleThread> worker = make_scoped_ptr(
        new base::DelegateSimpleThread(
            this,
            thread_name_prefix +
            base::StringPrintf(
                "Worker%u",
                static_cast<unsigned>(workers_.size() + 1)).c_str()));
    worker->Start();
#if defined(OS_ANDROID) || defined(OS_LINUX)
    worker->SetThreadPriority(base::kThreadPriority_Background);
#endif
    workers_.push_back(worker.Pass());
  }
}

TaskGraphRunner::~TaskGraphRunner() {
  {
    base::AutoLock lock(lock_);

    DCHECK_EQ(0u, ready_to_run_namespaces_.size());
    DCHECK_EQ(0u, namespaces_.size());

    DCHECK(!shutdown_);
    shutdown_ = true;

    // Wake up a worker so it knows it should exit. This will cause all workers
    // to exit as each will wake up another worker before exiting.
    has_ready_to_run_tasks_cv_.Signal();
  }

  while (workers_.size()) {
    scoped_ptr<base::DelegateSimpleThread> worker = workers_.take_front();
    // http://crbug.com/240453 - Join() is considered IO and will block this
    // thread. See also http://crbug.com/239423 for further ideas.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    worker->Join();
  }
}

void TaskGraphRunner::Register(const WorkerPool* worker_pool) {
  base::AutoLock lock(lock_);

  DCHECK(namespaces_.find(worker_pool) == namespaces_.end());
  linked_ptr<TaskNamespace> task_set = make_linked_ptr(new TaskNamespace());
  namespaces_[worker_pool] = task_set;
}

void TaskGraphRunner::Unregister(const WorkerPool* worker_pool) {
  base::AutoLock lock(lock_);

  DCHECK(namespaces_.find(worker_pool) != namespaces_.end());
  DCHECK_EQ(0u, namespaces_[worker_pool]->pending_tasks.size());
  DCHECK_EQ(0u, namespaces_[worker_pool]->ready_to_run_tasks.size());
  DCHECK_EQ(0u, namespaces_[worker_pool]->running_tasks.size());
  DCHECK_EQ(0u, namespaces_[worker_pool]->completed_tasks.size());

  namespaces_.erase(worker_pool);
}

void TaskGraphRunner::WaitForTasksToFinishRunning(
    const WorkerPool* worker_pool) {
  base::AutoLock lock(lock_);

  DCHECK(namespaces_.find(worker_pool) != namespaces_.end());
  TaskNamespace* task_namespace = namespaces_[worker_pool].get();

  while (!has_finished_running_tasks(task_namespace))
    has_namespaces_with_finished_running_tasks_cv_.Wait();

  // There may be other namespaces that have finished running
  // tasks, so wake up another origin thread.
  has_namespaces_with_finished_running_tasks_cv_.Signal();
}

void TaskGraphRunner::SetTaskGraph(const WorkerPool* worker_pool,
                                   TaskGraph* graph) {
  TaskGraph new_pending_tasks;
  TaskGraph new_running_tasks;

  new_pending_tasks.swap(*graph);

  {
    base::AutoLock lock(lock_);

    DCHECK(!shutdown_);
    DCHECK(namespaces_.find(worker_pool) != namespaces_.end());
    TaskNamespace* task_namespace = namespaces_[worker_pool].get();

    // First remove all completed tasks from |new_pending_tasks| and
    // adjust number of dependencies.
    for (TaskVector::iterator it = task_namespace->completed_tasks.begin();
         it != task_namespace->completed_tasks.end(); ++it) {
      internal::WorkerPoolTask* task = it->get();

      scoped_ptr<internal::GraphNode> node = new_pending_tasks.take_and_erase(
          task);
      if (node) {
        for (internal::GraphNode::Vector::const_iterator it =
                 node->dependents().begin();
             it != node->dependents().end(); ++it) {
          internal::GraphNode* dependent_node = *it;
          dependent_node->remove_dependency();
        }
      }
    }

    // Build new running task set.
    for (TaskGraph::iterator it = task_namespace->running_tasks.begin();
         it != task_namespace->running_tasks.end(); ++it) {
      internal::WorkerPoolTask* task = it->first;
      // Transfer scheduled task value from |new_pending_tasks| to
      // |new_running_tasks| if currently running. Value must be set to
      // NULL if |new_pending_tasks| doesn't contain task. This does
      // the right in both cases.
      new_running_tasks.set(task, new_pending_tasks.take_and_erase(task));
    }

    // Build new "ready to run" tasks queue.
    task_namespace->ready_to_run_tasks.clear();
    for (TaskGraph::iterator it = new_pending_tasks.begin();
         it != new_pending_tasks.end(); ++it) {
      internal::WorkerPoolTask* task = it->first;
      DCHECK(task);
      internal::GraphNode* node = it->second;

      // Completed tasks should not exist in |new_pending_tasks|.
      DCHECK(!task->HasFinishedRunning());

      // Call DidSchedule() to indicate that this task has been scheduled.
      // Note: This is only for debugging purposes.
      task->DidSchedule();

      if (!node->num_dependencies())
        task_namespace->ready_to_run_tasks.push_back(node);

      // Erase the task from old pending tasks.
      task_namespace->pending_tasks.erase(task);
    }

    // Rearrange the elements in |ready_to_run_tasks| in such a way that
    // they form a heap.
    std::make_heap(task_namespace->ready_to_run_tasks.begin(),
                   task_namespace->ready_to_run_tasks.end(),
                   CompareTaskPriority);

    task_namespace->completed_tasks.reserve(
        task_namespace->completed_tasks.size() +
        task_namespace->pending_tasks.size());

    // The items left in |pending_tasks| need to be canceled.
    for (TaskGraph::const_iterator it = task_namespace->pending_tasks.begin();
         it != task_namespace->pending_tasks.end(); ++it) {
      task_namespace->completed_tasks.push_back(it->first);
    }

    // Swap task sets.
    // Note: old tasks are intentionally destroyed after releasing |lock_|.
    task_namespace->pending_tasks.swap(new_pending_tasks);
    task_namespace->running_tasks.swap(new_running_tasks);

    // If |ready_to_run_tasks| is empty, it means we either have
    // running tasks, or we have no pending tasks.
    DCHECK(!task_namespace->ready_to_run_tasks.empty() ||
           (task_namespace->pending_tasks.empty() ||
            !task_namespace->running_tasks.empty()));

    // Build new "ready to run" task namespaces queue.
    ready_to_run_namespaces_.clear();
    for (TaskNamespaceMap::iterator it = namespaces_.begin();
         it != namespaces_.end(); ++it) {
      if (!it->second->ready_to_run_tasks.empty())
        ready_to_run_namespaces_.push_back(it->second.get());
    }

    // Rearrange the task namespaces in |ready_to_run_namespaces_|
    // in such a way that they form a heap.
    std::make_heap(ready_to_run_namespaces_.begin(),
                   ready_to_run_namespaces_.end(),
                   CompareTaskNamespacePriority);

    // If there is more work available, wake up worker thread.
    if (!ready_to_run_namespaces_.empty())
      has_ready_to_run_tasks_cv_.Signal();
  }
}

void TaskGraphRunner::CollectCompletedTasks(
    const WorkerPool* worker_pool, TaskVector* completed_tasks) {
  base::AutoLock lock(lock_);

  DCHECK_EQ(0u, completed_tasks->size());
  DCHECK(namespaces_.find(worker_pool) != namespaces_.end());
  completed_tasks->swap(namespaces_[worker_pool]->completed_tasks);
}

void TaskGraphRunner::Run() {
  base::AutoLock lock(lock_);

  // Get a unique thread index.
  int thread_index = next_thread_index_++;

  while (true) {
    if (ready_to_run_namespaces_.empty()) {
      // Exit when shutdown is set and no more tasks are pending.
      if (shutdown_)
        break;

      // Wait for more tasks.
      has_ready_to_run_tasks_cv_.Wait();
      continue;
    }

    // Take top priority TaskNamespace from |ready_to_run_namespaces_|.
    std::pop_heap(ready_to_run_namespaces_.begin(),
                  ready_to_run_namespaces_.end(),
                  CompareTaskNamespacePriority);
    TaskNamespace* task_namespace = ready_to_run_namespaces_.back();
    ready_to_run_namespaces_.pop_back();
    DCHECK(!task_namespace->ready_to_run_tasks.empty());

    // Take top priority task from |ready_to_run_tasks|.
    std::pop_heap(task_namespace->ready_to_run_tasks.begin(),
                  task_namespace->ready_to_run_tasks.end(),
                  CompareTaskPriority);
    scoped_refptr<internal::WorkerPoolTask> task(
        task_namespace->ready_to_run_tasks.back()->task());
    task_namespace->ready_to_run_tasks.pop_back();

    // Add task namespace back to |ready_to_run_namespaces_| if not
    // empty after taking top priority task.
    if (!task_namespace->ready_to_run_tasks.empty()) {
      ready_to_run_namespaces_.push_back(task_namespace);
      std::push_heap(ready_to_run_namespaces_.begin(),
                     ready_to_run_namespaces_.end(),
                     CompareTaskNamespacePriority);
    }

    // Move task from |pending_tasks| to |running_tasks|.
    DCHECK(task_namespace->pending_tasks.contains(task.get()));
    DCHECK(!task_namespace->running_tasks.contains(task.get()));
    task_namespace->running_tasks.set(
        task.get(),
        task_namespace->pending_tasks.take_and_erase(task.get()));

    // There may be more work available, so wake up another worker thread.
    has_ready_to_run_tasks_cv_.Signal();

    // Call WillRun() before releasing |lock_| and running task.
    task->WillRun();

    {
      base::AutoUnlock unlock(lock_);

      task->RunOnWorkerThread(thread_index);
    }

    // This will mark task as finished running.
    task->DidRun();

    // Now iterate over all dependents to remove dependency and check
    // if they are ready to run.
    scoped_ptr<internal::GraphNode> node =
        task_namespace->running_tasks.take_and_erase(task.get());
    if (node) {
      bool ready_to_run_namespaces_has_heap_properties = true;

      for (internal::GraphNode::Vector::const_iterator it =
               node->dependents().begin();
           it != node->dependents().end(); ++it) {
        internal::GraphNode* dependent_node = *it;

        dependent_node->remove_dependency();
        // Task is ready if it has no dependencies. Add it to
        // |ready_to_run_tasks_|.
        if (!dependent_node->num_dependencies()) {
          bool was_empty = task_namespace->ready_to_run_tasks.empty();
          task_namespace->ready_to_run_tasks.push_back(dependent_node);
          std::push_heap(task_namespace->ready_to_run_tasks.begin(),
                         task_namespace->ready_to_run_tasks.end(),
                         CompareTaskPriority);
          // Task namespace is ready if it has at least one ready
          // to run task. Add it to |ready_to_run_namespaces_| if
          // it just become ready.
          if (was_empty) {
            DCHECK(std::find(ready_to_run_namespaces_.begin(),
                             ready_to_run_namespaces_.end(),
                             task_namespace) ==
                   ready_to_run_namespaces_.end());
            ready_to_run_namespaces_.push_back(task_namespace);
          }
          ready_to_run_namespaces_has_heap_properties = false;
        }
      }

      // Rearrange the task namespaces in |ready_to_run_namespaces_|
      // in such a way that they yet again form a heap.
      if (!ready_to_run_namespaces_has_heap_properties) {
        std::make_heap(ready_to_run_namespaces_.begin(),
                       ready_to_run_namespaces_.end(),
                       CompareTaskNamespacePriority);
      }
    }

    // Finally add task to |completed_tasks_|.
    task_namespace->completed_tasks.push_back(task);

    // If namespace has finished running all tasks, wake up origin thread.
    if (has_finished_running_tasks(task_namespace))
      has_namespaces_with_finished_running_tasks_cv_.Signal();
  }

  // We noticed we should exit. Wake up the next worker so it knows it should
  // exit as well (because the Shutdown() code only signals once).
  has_ready_to_run_tasks_cv_.Signal();
}

class CompositorRasterTaskGraphRunner
    : public TaskGraphRunner {
 public:
  CompositorRasterTaskGraphRunner() : TaskGraphRunner(
      WorkerPool::GetNumRasterThreads(), "CompositorRaster") {
  }
};

base::LazyInstance<CompositorRasterTaskGraphRunner>
    g_task_graph_runner = LAZY_INSTANCE_INITIALIZER;

const int kDefaultNumRasterThreads = 1;

int g_num_raster_threads = 0;

}  // namespace

namespace internal {

WorkerPoolTask::WorkerPoolTask()
    : did_schedule_(false),
      did_run_(false),
      did_complete_(false) {
}

WorkerPoolTask::~WorkerPoolTask() {
  DCHECK_EQ(did_schedule_, did_complete_);
  DCHECK(!did_run_ || did_schedule_);
  DCHECK(!did_run_ || did_complete_);
}

void WorkerPoolTask::DidSchedule() {
  DCHECK(!did_complete_);
  did_schedule_ = true;
}

void WorkerPoolTask::WillRun() {
  DCHECK(did_schedule_);
  DCHECK(!did_complete_);
  DCHECK(!did_run_);
}

void WorkerPoolTask::DidRun() {
  did_run_ = true;
}

void WorkerPoolTask::WillComplete() {
  DCHECK(!did_complete_);
}

void WorkerPoolTask::DidComplete() {
  DCHECK(did_schedule_);
  DCHECK(!did_complete_);
  did_complete_ = true;
}

bool WorkerPoolTask::HasFinishedRunning() const {
  return did_run_;
}

bool WorkerPoolTask::HasCompleted() const {
  return did_complete_;
}

GraphNode::GraphNode(internal::WorkerPoolTask* task, unsigned priority)
    : task_(task),
      priority_(priority),
      num_dependencies_(0) {
}

GraphNode::~GraphNode() {
}

}  // namespace internal

// static
void WorkerPool::SetNumRasterThreads(int num_threads) {
  DCHECK_LT(0, num_threads);
  DCHECK_EQ(0, g_num_raster_threads);

  g_num_raster_threads = num_threads;
}

// static
int WorkerPool::GetNumRasterThreads() {
  if (!g_num_raster_threads)
    g_num_raster_threads = kDefaultNumRasterThreads;

  return g_num_raster_threads;
}

WorkerPool::WorkerPool() : in_dispatch_completion_callbacks_(false) {
  g_task_graph_runner.Pointer()->Register(this);
}

WorkerPool::~WorkerPool() {
  g_task_graph_runner.Pointer()->Unregister(this);
}

void WorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "WorkerPool::Shutdown");

  DCHECK(!in_dispatch_completion_callbacks_);

  g_task_graph_runner.Pointer()->WaitForTasksToFinishRunning(this);
}

void WorkerPool::SetTaskGraph(TaskGraph* graph) {
  TRACE_EVENT1("cc", "WorkerPool::SetTaskGraph",
               "num_tasks", graph->size());

  DCHECK(!in_dispatch_completion_callbacks_);

  g_task_graph_runner.Pointer()->SetTaskGraph(this, graph);
}

void WorkerPool::CheckForCompletedWorkerTasks() {
  TRACE_EVENT0("cc", "WorkerPool::CheckForCompletedWorkerTasks");

  DCHECK(!in_dispatch_completion_callbacks_);

  TaskVector completed_tasks;
  g_task_graph_runner.Pointer()->CollectCompletedTasks(this, &completed_tasks);
  ProcessCompletedTasks(completed_tasks);
}

void WorkerPool::ProcessCompletedTasks(
    const TaskVector& completed_tasks) {
  TRACE_EVENT1("cc", "WorkerPool::ProcessCompletedTasks",
               "completed_task_count", completed_tasks.size());

  // Worker pool instance is not reentrant while processing completed tasks.
  in_dispatch_completion_callbacks_ = true;

  for (TaskVector::const_iterator it = completed_tasks.begin();
       it != completed_tasks.end();
       ++it) {
    internal::WorkerPoolTask* task = it->get();

    task->WillComplete();
    task->CompleteOnOriginThread();
    task->DidComplete();
  }

  in_dispatch_completion_callbacks_ = false;
}

}  // namespace cc
