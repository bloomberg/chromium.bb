// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_GRAPH_OBSERVER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_GRAPH_OBSERVER_H_

#include "base/macros.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace performance_manager {

class FrameNodeImpl;
class Graph;
class NodeBase;
class PageNodeImpl;
class ProcessNodeImpl;
class SystemNodeImpl;

// An observer API for the graph.
//
// Observers are generally instantiated when the graph is empty, and outlive it,
// though it's valid for an observer to be registered at any time. Observers
// must unregister before they're destroyed.
//
// To create and install a new observer:
//   (1) Derive from this class.
//   (2) Register by calling on |graph().RegisterObserver|.
//   (3) Before destruction, unregister by calling on
//       |graph().UnregisterObserver|.
//
// TODO: Clean up the observer API, and create a wrapper version that sees
// const Node* rather then mutable NodeImpl* types for external consumers.
class GraphObserver {
 public:
  GraphObserver();
  virtual ~GraphObserver();

  // Invoked when an observer is added to or removed from the graph. This is a
  // convenient place for observers to initialize any necessary state, validate
  // graph invariants, etc.
  virtual void OnRegistered() {}
  virtual void OnUnregistered() {}

  // Determines whether or not the observer should be registered with, and
  // invoked for, the |node|.
  virtual bool ShouldObserve(const NodeBase* node) = 0;

  // Called whenever a node has been added to the graph.
  virtual void OnNodeAdded(NodeBase* node) {}

  // Called when the |node| is about to be removed from the graph.
  virtual void OnBeforeNodeRemoved(NodeBase* node) {}

  // FrameNodeImpl notifications.
  virtual void OnIsCurrentChanged(FrameNodeImpl* frame_node) {}
  virtual void OnNetworkAlmostIdleChanged(FrameNodeImpl* frame_node) {}
  virtual void OnLifecycleStateChanged(FrameNodeImpl* frame_node) {}
  virtual void OnNonPersistentNotificationCreated(FrameNodeImpl* frame_node) {}

  // PageNodeImpl notifications.
  virtual void OnIsVisibleChanged(PageNodeImpl* page_node) {}
  virtual void OnIsLoadingChanged(PageNodeImpl* page_node) {}
  virtual void OnUkmSourceIdChanged(PageNodeImpl* page_node) {}
  virtual void OnLifecycleStateChanged(PageNodeImpl* page_node) {}
  virtual void OnPageAlmostIdleChanged(PageNodeImpl* page_node) {}
  virtual void OnFaviconUpdated(PageNodeImpl* page_node) {}
  virtual void OnTitleUpdated(PageNodeImpl* page_node) {}
  virtual void OnMainFrameNavigationCommitted(PageNodeImpl* page_node) {}

  // ProcessNodeImpl notifications.
  virtual void OnExpectedTaskQueueingDurationSample(
      ProcessNodeImpl* process_node) {}
  virtual void OnMainThreadTaskLoadIsLow(ProcessNodeImpl* process_node) {}
  virtual void OnRendererIsBloated(ProcessNodeImpl* process_node) {}
  virtual void OnAllFramesInProcessFrozen(ProcessNodeImpl* process_node) {}

  // SystemNodeImpl notifications.
  virtual void OnProcessCPUUsageReady(SystemNodeImpl* system_node) {}

  void set_node_graph(Graph* graph) { node_graph_ = graph; }

  Graph* graph() const { return node_graph_; }

 private:
  Graph* node_graph_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GraphObserver);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_GRAPH_OBSERVER_H_
