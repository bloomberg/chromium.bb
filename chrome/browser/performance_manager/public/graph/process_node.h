// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PROCESS_NODE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PROCESS_NODE_H_

#include "base/macros.h"
#include "chrome/browser/performance_manager/public/graph/node.h"

namespace performance_manager {

class ProcessNodeObserver;

// A process node follows the lifetime of a RenderProcessHost.
// It may reference zero or one processes at a time, but during its lifetime, it
// may reference more than one process. This can happen if the associated
// renderer crashes, and an associated frame is then reloaded or re-navigated.
// The state of the process node goes through:
// 1. Created, no PID.
// 2. Process started, have PID - in the case where the associated render
//    process fails to start, this state may not occur.
// 3. Process died or failed to start, have exit status.
// 4. Back to 2.
class ProcessNode : public Node {
 public:
  using Observer = ProcessNodeObserver;
  class ObserverDefaultImpl;

  ProcessNode();
  ~ProcessNode() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessNode);
};

// Pure virtual observer interface. Derive from this if you want to be forced to
// implement the entire interface.
class ProcessNodeObserver {
 public:
  ProcessNodeObserver();
  virtual ~ProcessNodeObserver();

  // Node lifetime notifications.

  // Called when a |process_node| is added to the graph.
  virtual void OnProcessNodeAdded(const ProcessNode* process_node) = 0;

  // Called before a |process_node| is removed from the graph.
  virtual void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) = 0;

  // Notifications of property changes.

  // Invoked when a new |expected_task_queueing_duration| sample is available.
  virtual void OnExpectedTaskQueueingDurationSample(
      const ProcessNode* process_node) = 0;

  // Invoked when the |main_thread_task_load_is_low| property changes.
  virtual void OnMainThreadTaskLoadIsLow(const ProcessNode* process_node) = 0;

  // Events with no property changes.

  // Fired when all frames in a process have transitioned to being frozen.
  virtual void OnAllFramesInProcessFrozen(const ProcessNode* process_node) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessNodeObserver);
};

// Default implementation of observer that provides dummy versions of each
// function. Derive from this if you only need to implement a few of the
// functions.
class ProcessNode::ObserverDefaultImpl : public ProcessNodeObserver {
 public:
  ObserverDefaultImpl();
  ~ObserverDefaultImpl() override;

  // ProcessNodeObserver implementation:
  void OnProcessNodeAdded(const ProcessNode* process_node) override {}
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override {}
  void OnExpectedTaskQueueingDurationSample(
      const ProcessNode* process_node) override {}
  void OnMainThreadTaskLoadIsLow(const ProcessNode* process_node) override {}
  void OnAllFramesInProcessFrozen(const ProcessNode* process_node) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ObserverDefaultImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PROCESS_NODE_H_
