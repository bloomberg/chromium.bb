// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TASK_GRAPH_RUNNER_H_
#define CC_RESOURCES_TASK_GRAPH_RUNNER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/synchronization/condition_variable.h"
#include "base/threading/simple_thread.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_deque.h"

namespace cc {
namespace internal {

class CC_EXPORT Task : public base::RefCountedThreadSafe<Task> {
 public:
  typedef std::vector<scoped_refptr<Task> > Vector;

  virtual void RunOnWorkerThread(unsigned thread_index) = 0;

  void WillRun();
  void DidRun();
  bool HasFinishedRunning() const;

 protected:
  friend class base::RefCountedThreadSafe<Task>;

  Task();
  virtual ~Task();

  bool did_run_;
};

// Dependencies are represented as edges in a task graph. Each graph node
// is assigned a priority and a run count that matches the number of
// dependencies.
struct CC_EXPORT TaskGraph {
  struct Node {
    class TaskComparator {
     public:
      explicit TaskComparator(const Task* task) : task_(task) {}

      bool operator()(const Node& node) const { return node.task == task_; }

     private:
      const Task* task_;
    };

    typedef std::vector<Node> Vector;

    Node(Task* task, unsigned priority, size_t dependencies)
        : task(task), priority(priority), dependencies(dependencies) {}

    Task* task;
    unsigned priority;
    size_t dependencies;
  };

  struct Edge {
    typedef std::vector<Edge> Vector;

    Edge(const Task* task, Task* dependent)
        : task(task), dependent(dependent) {}

    const Task* task;
    Task* dependent;
  };

  TaskGraph();
  ~TaskGraph();

  void Swap(TaskGraph* other);
  void Reset();

  Node::Vector nodes;
  Edge::Vector edges;
};

class TaskGraphRunner;

// Opaque identifier that defines a namespace of tasks.
class CC_EXPORT NamespaceToken {
 public:
  NamespaceToken() : id_(0) {}
  ~NamespaceToken() {}

  bool IsValid() const { return id_ != 0; }

 private:
  friend class TaskGraphRunner;

  explicit NamespaceToken(int id) : id_(id) {}

  int id_;
};

// A worker thread pool that runs tasks provided by task graph. Destructor
// might block and should not be used on a thread that needs to be responsive.
class CC_EXPORT TaskGraphRunner : public base::DelegateSimpleThread::Delegate {
 public:
  TaskGraphRunner(size_t num_threads, const std::string& thread_name_prefix);
  virtual ~TaskGraphRunner();

  // Returns a unique token that can be used to pass a task graph to
  // SetTaskGraph(). Valid tokens are always nonzero.
  NamespaceToken GetNamespaceToken();

  // Schedule running of tasks in |graph|. Tasks previously scheduled but
  // no longer needed will be canceled unless already running. Canceled
  // tasks are moved to |completed_tasks| without being run. The result
  // is that once scheduled, a task is guaranteed to end up in the
  // |completed_tasks| queue even if it later gets canceled by another
  // call to SetTaskGraph().
  void SetTaskGraph(NamespaceToken token, TaskGraph* graph);

  // Wait for all scheduled tasks to finish running.
  void WaitForTasksToFinishRunning(NamespaceToken token);

  // Collect all completed tasks in |completed_tasks|.
  void CollectCompletedTasks(NamespaceToken token,
                             Task::Vector* completed_tasks);

  // Run one task on current thread. Returns false if no tasks are ready
  // to run. This should only be used by tests.
  bool RunTaskForTesting();

 private:
  struct PrioritizedTask {
    typedef std::vector<PrioritizedTask> Vector;

    PrioritizedTask(Task* task, unsigned priority)
        : task(task), priority(priority) {}

    Task* task;
    unsigned priority;
  };

  struct TaskNamespace {
    typedef std::vector<TaskNamespace*> Vector;

    TaskNamespace();
    ~TaskNamespace();

    // Current task graph.
    TaskGraph graph;

    // Ordered set of tasks that are ready to run.
    PrioritizedTask::Vector ready_to_run_tasks;

    // Completed tasks not yet collected by origin thread.
    Task::Vector completed_tasks;

    // Number of currently running tasks.
    size_t num_running_tasks;
  };

  typedef std::map<int, TaskNamespace> TaskNamespaceMap;

  static bool CompareTaskPriority(const PrioritizedTask& a,
                                  const PrioritizedTask& b) {
    // In this system, numerically lower priority is run first.
    return a.priority > b.priority;
  }

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

  static bool HasFinishedRunningTasksInNamespace(
      const TaskNamespace* task_namespace) {
    return !task_namespace->num_running_tasks &&
           task_namespace->ready_to_run_tasks.empty();
  }

  // Overridden from base::DelegateSimpleThread:
  virtual void Run() OVERRIDE;

  // Run next task. Caller must acquire |lock_| prior to calling this
  // function and make sure at least one task is ready to run.
  void RunTaskWithLockAcquired(int thread_index);

  // This lock protects all members of this class. Do not read or modify
  // anything without holding this lock. Do not block while holding this
  // lock.
  mutable base::Lock lock_;

  // Condition variable that is waited on by worker threads until new
  // tasks are ready to run or shutdown starts.
  base::ConditionVariable has_ready_to_run_tasks_cv_;

  // Condition variable that is waited on by origin threads until a
  // namespace has finished running all associated tasks.
  base::ConditionVariable has_namespaces_with_finished_running_tasks_cv_;

  // Provides a unique id to each NamespaceToken.
  int next_namespace_id_;

  // This set contains all namespaces with pending, running or completed
  // tasks not yet collected.
  TaskNamespaceMap namespaces_;

  // Ordered set of task namespaces that have ready to run tasks.
  TaskNamespace::Vector ready_to_run_namespaces_;

  // Provides each running thread loop with a unique index. First thread
  // loop index is 0.
  unsigned next_thread_index_;

  // This set contains all currently running tasks.
  typedef std::vector<const Task*> TaskVector;
  TaskVector running_tasks_;

  // Set during shutdown. Tells workers to exit when no more tasks
  // are pending.
  bool shutdown_;

  ScopedPtrDeque<base::DelegateSimpleThread> workers_;

  DISALLOW_COPY_AND_ASSIGN(TaskGraphRunner);
};

}  // namespace internal
}  // namespace cc

#endif  // CC_RESOURCES_TASK_GRAPH_RUNNER_H_
