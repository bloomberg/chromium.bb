// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/task_graph_work_queue.h"

#include <algorithm>
#include <utility>

#include "base/trace_event/trace_event.h"

namespace cc {

TaskGraphWorkQueue::TaskNamespace::TaskNamespace() {}

TaskGraphWorkQueue::TaskNamespace::~TaskNamespace() {}

TaskGraphWorkQueue::TaskGraphWorkQueue() : next_namespace_id_(1) {}
TaskGraphWorkQueue::~TaskGraphWorkQueue() {}

NamespaceToken TaskGraphWorkQueue::GetNamespaceToken() {
  NamespaceToken token(next_namespace_id_++);
  DCHECK(namespaces_.find(token) == namespaces_.end());
  return token;
}

void TaskGraphWorkQueue::ScheduleTasks(NamespaceToken token, TaskGraph* graph) {
  TaskNamespace& task_namespace = namespaces_[token];

  // First adjust number of dependencies to reflect completed tasks.
  for (const scoped_refptr<Task>& task : task_namespace.completed_tasks) {
    for (DependentIterator node_it(graph, task.get()); node_it; ++node_it) {
      TaskGraph::Node& node = *node_it;
      DCHECK_LT(0u, node.dependencies);
      node.dependencies--;
    }
  }

  // Build new "ready to run" queue and remove nodes from old graph.
  task_namespace.ready_to_run_tasks.clear();
  for (const TaskGraph::Node& node : graph->nodes) {
    // Remove any old nodes that are associated with this task. The result is
    // that the old graph is left with all nodes not present in this graph,
    // which we use below to determine what tasks need to be canceled.
    TaskGraph::Node::Vector::iterator old_it = std::find_if(
        task_namespace.graph.nodes.begin(), task_namespace.graph.nodes.end(),
        [node](const TaskGraph::Node& other) {
          return node.task == other.task;
        });
    if (old_it != task_namespace.graph.nodes.end()) {
      std::swap(*old_it, task_namespace.graph.nodes.back());
      task_namespace.graph.nodes.pop_back();
    }

    // Task is not ready to run if dependencies are not yet satisfied.
    if (node.dependencies)
      continue;

    // Skip if already finished running task.
    if (node.task->HasFinishedRunning())
      continue;

    // Skip if already running.
    if (std::find(task_namespace.running_tasks.begin(),
                  task_namespace.running_tasks.end(),
                  node.task) != task_namespace.running_tasks.end())
      continue;

    task_namespace.ready_to_run_tasks.push_back(
        PrioritizedTask(node.task, &task_namespace, node.priority));
  }

  // Rearrange the elements in |ready_to_run_tasks| in such a way that they
  // form a heap.
  std::make_heap(task_namespace.ready_to_run_tasks.begin(),
                 task_namespace.ready_to_run_tasks.end(), CompareTaskPriority);

  // Swap task graph.
  task_namespace.graph.Swap(graph);

  // Determine what tasks in old graph need to be canceled.
  for (TaskGraph::Node::Vector::iterator it = graph->nodes.begin();
       it != graph->nodes.end(); ++it) {
    TaskGraph::Node& node = *it;

    // Skip if already finished running task.
    if (node.task->HasFinishedRunning())
      continue;

    // Skip if already running.
    if (std::find(task_namespace.running_tasks.begin(),
                  task_namespace.running_tasks.end(),
                  node.task) != task_namespace.running_tasks.end())
      continue;

    DCHECK(std::find(task_namespace.completed_tasks.begin(),
                     task_namespace.completed_tasks.end(),
                     node.task) == task_namespace.completed_tasks.end());
    task_namespace.completed_tasks.push_back(node.task);
  }

  // Build new "ready to run" task namespaces queue.
  ready_to_run_namespaces_.clear();
  for (auto& it : namespaces_) {
    if (!it.second.ready_to_run_tasks.empty())
      ready_to_run_namespaces_.push_back(&it.second);
  }

  // Rearrange the task namespaces in |ready_to_run_namespaces| in such a
  // way that they form a heap.
  std::make_heap(ready_to_run_namespaces_.begin(),
                 ready_to_run_namespaces_.end(), CompareTaskNamespacePriority);
}

TaskGraphWorkQueue::PrioritizedTask TaskGraphWorkQueue::GetNextTaskToRun() {
  DCHECK(!ready_to_run_namespaces_.empty());

  // Take top priority TaskNamespace from |ready_to_run_namespaces_|.
  std::pop_heap(ready_to_run_namespaces_.begin(),
                ready_to_run_namespaces_.end(), CompareTaskNamespacePriority);
  TaskNamespace* task_namespace = ready_to_run_namespaces_.back();
  ready_to_run_namespaces_.pop_back();
  DCHECK(!task_namespace->ready_to_run_tasks.empty());

  // Take top priority task from |ready_to_run_tasks|.
  std::pop_heap(task_namespace->ready_to_run_tasks.begin(),
                task_namespace->ready_to_run_tasks.end(), CompareTaskPriority);
  PrioritizedTask task = task_namespace->ready_to_run_tasks.back();
  task_namespace->ready_to_run_tasks.pop_back();

  // Add task namespace back to |ready_to_run_namespaces_| if not empty after
  // taking top priority task.
  if (!task_namespace->ready_to_run_tasks.empty()) {
    ready_to_run_namespaces_.push_back(task_namespace);
    std::push_heap(ready_to_run_namespaces_.begin(),
                   ready_to_run_namespaces_.end(),
                   CompareTaskNamespacePriority);
  }

  // Add task to |running_tasks|.
  task_namespace->running_tasks.push_back(task.task);

  return task;
}

void TaskGraphWorkQueue::CompleteTask(const PrioritizedTask& completed_task) {
  TaskNamespace* task_namespace = completed_task.task_namespace;
  scoped_refptr<Task> task(completed_task.task);

  // Remove task from |running_tasks|.
  auto it = std::find(task_namespace->running_tasks.begin(),
                      task_namespace->running_tasks.end(), task);
  DCHECK(it != task_namespace->running_tasks.end());
  std::swap(*it, task_namespace->running_tasks.back());
  task_namespace->running_tasks.pop_back();

  // Now iterate over all dependents to decrement dependencies and check if they
  // are ready to run.
  bool ready_to_run_namespaces_has_heap_properties = true;
  for (DependentIterator it(&task_namespace->graph, task.get()); it; ++it) {
    TaskGraph::Node& dependent_node = *it;

    DCHECK_LT(0u, dependent_node.dependencies);
    dependent_node.dependencies--;
    // Task is ready if it has no dependencies. Add it to |ready_to_run_tasks_|.
    if (!dependent_node.dependencies) {
      bool was_empty = task_namespace->ready_to_run_tasks.empty();
      task_namespace->ready_to_run_tasks.push_back(PrioritizedTask(
          dependent_node.task, task_namespace, dependent_node.priority));
      std::push_heap(task_namespace->ready_to_run_tasks.begin(),
                     task_namespace->ready_to_run_tasks.end(),
                     CompareTaskPriority);
      // Task namespace is ready if it has at least one ready to run task. Add
      // it to |ready_to_run_namespaces_| if it just become ready.
      if (was_empty) {
        DCHECK(std::find(ready_to_run_namespaces_.begin(),
                         ready_to_run_namespaces_.end(),
                         task_namespace) == ready_to_run_namespaces_.end());
        ready_to_run_namespaces_.push_back(task_namespace);
      }
      ready_to_run_namespaces_has_heap_properties = false;
    }
  }

  // Rearrange the task namespaces in |ready_to_run_namespaces_| in such a way
  // that they yet again form a heap.
  if (!ready_to_run_namespaces_has_heap_properties) {
    std::make_heap(ready_to_run_namespaces_.begin(),
                   ready_to_run_namespaces_.end(),
                   CompareTaskNamespacePriority);
  }

  // Finally add task to |completed_tasks_|.
  task_namespace->completed_tasks.push_back(task);
}

void TaskGraphWorkQueue::CollectCompletedTasks(NamespaceToken token,
                                               Task::Vector* completed_tasks) {
  TaskNamespaceMap::iterator it = namespaces_.find(token);
  if (it == namespaces_.end())
    return;

  TaskNamespace& task_namespace = it->second;

  DCHECK_EQ(0u, completed_tasks->size());
  completed_tasks->swap(task_namespace.completed_tasks);
  if (!HasFinishedRunningTasksInNamespace(&task_namespace))
    return;

  // Remove namespace if finished running tasks.
  DCHECK_EQ(0u, task_namespace.completed_tasks.size());
  DCHECK_EQ(0u, task_namespace.ready_to_run_tasks.size());
  DCHECK_EQ(0u, task_namespace.running_tasks.size());
  namespaces_.erase(it);
}

bool TaskGraphWorkQueue::DependencyMismatch(const TaskGraph* graph) {
  // Value storage will be 0-initialized.
  base::hash_map<const Task*, size_t> dependents;
  for (const TaskGraph::Edge& edge : graph->edges)
    dependents[edge.dependent]++;

  for (const TaskGraph::Node& node : graph->nodes) {
    if (dependents[node.task] != node.dependencies)
      return true;
  }

  return false;
}

}  // namespace cc
