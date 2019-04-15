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

// An observer API for the coordination unit graph maintained by GRC.
//
// Observers are instantiated when the resource_coordinator service
// is created and are destroyed when the resource_coordinator service
// is destroyed. Therefore observers are guaranteed to be alive before
// any coordination unit is created and will be alive after any
// coordination unit is destroyed. Additionally, any
// Coordination Unit reachable within a callback will always be
// initialized and valid.
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

  // Determines whether or not the observer should be registered with, and
  // invoked for, the |node|.
  virtual bool ShouldObserve(const NodeBase* node) = 0;

  // Called whenever a CoordinationUnit is created.
  virtual void OnNodeAdded(NodeBase* node) {}

  // Called when the |node| is about to be destroyed.
  virtual void OnBeforeNodeRemoved(NodeBase* node) {}

  // Called whenever an event is received in |node| if the
  // |node| doesn't implement its own EventReceived handler.
  // TODO(chrisha): Kill these generic event handlers in favor of specific
  // event handlers.
  virtual void OnEventReceived(NodeBase* node,
                               resource_coordinator::mojom::Event event) {}
  virtual void OnFrameEventReceived(FrameNodeImpl* frame_node,
                                    resource_coordinator::mojom::Event event) {}
  virtual void OnPageEventReceived(PageNodeImpl* page_node,
                                   resource_coordinator::mojom::Event event) {}
  virtual void OnProcessEventReceived(
      ProcessNodeImpl* process_node,
      resource_coordinator::mojom::Event event) {}
  virtual void OnSystemEventReceived(SystemNodeImpl* system_node,
                                     resource_coordinator::mojom::Event event) {
  }

  // FrameNodeImpl notifications.
  virtual void OnIsCurrentChanged(FrameNodeImpl* frame_node) {}
  virtual void OnNetworkAlmostIdleChanged(FrameNodeImpl* frame_node) {}
  virtual void OnLifecycleStateChanged(FrameNodeImpl* frame_node) {}

  // PageNodeImpl notifications.
  virtual void OnIsVisibleChanged(PageNodeImpl* page_node) {}
  virtual void OnIsLoadingChanged(PageNodeImpl* page_node) {}
  virtual void OnUkmSourceIdChanged(PageNodeImpl* page_node) {}
  virtual void OnLifecycleStateChanged(PageNodeImpl* page_node) {}

  // ProcessNodeImpl notifications.
  virtual void OnExpectedTaskQueueingDurationSample(
      ProcessNodeImpl* process_node) {}
  virtual void OnMainThreadTaskLoadIsLow(ProcessNodeImpl* process_node) {}

  // Called when page almost idle state changes. This is a computed property and
  // will only be maintained if a PageAlmostIdleDecorator exists on the graph.
  virtual void OnPageAlmostIdleChanged(PageNodeImpl* page_node) {}

  // Called when all the frames in a process become frozen.
  virtual void OnAllFramesInProcessFrozen(ProcessNodeImpl* process_node) {}

  void set_node_graph(Graph* graph) { node_graph_ = graph; }

  Graph* graph() const { return node_graph_; }

 private:
  Graph* node_graph_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GraphObserver);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_GRAPH_OBSERVER_H_
