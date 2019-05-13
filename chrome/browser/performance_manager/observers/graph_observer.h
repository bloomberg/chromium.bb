// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_GRAPH_OBSERVER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_GRAPH_OBSERVER_H_

#include "base/macros.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace performance_manager {

class FrameNodeImpl;
class GraphImpl;
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
  virtual ~GraphObserver() = 0;

  // Invoked when an observer is added to or removed from the graph. This is a
  // convenient place for observers to initialize any necessary state, validate
  // graph invariants, etc.
  virtual void OnRegistered() = 0;
  virtual void OnUnregistered() = 0;

  // Determines whether or not the observer should be registered with, and
  // invoked for, the |node|.
  virtual bool ShouldObserve(const NodeBase* node) = 0;

  // Called whenever a node has been added to the graph.
  virtual void OnNodeAdded(NodeBase* node) = 0;

  // Called when the |node| is about to be removed from the graph.
  virtual void OnBeforeNodeRemoved(NodeBase* node) = 0;

  // FrameNodeImpl notifications.
  virtual void OnIsCurrentChanged(FrameNodeImpl* frame_node) = 0;
  virtual void OnNetworkAlmostIdleChanged(FrameNodeImpl* frame_node) = 0;
  virtual void OnLifecycleStateChanged(FrameNodeImpl* frame_node) = 0;
  virtual void OnNonPersistentNotificationCreated(
      FrameNodeImpl* frame_node) = 0;

  // PageNodeImpl notifications.
  virtual void OnIsVisibleChanged(PageNodeImpl* page_node) = 0;
  virtual void OnIsLoadingChanged(PageNodeImpl* page_node) = 0;
  virtual void OnUkmSourceIdChanged(PageNodeImpl* page_node) = 0;
  virtual void OnLifecycleStateChanged(PageNodeImpl* page_node) = 0;
  virtual void OnPageAlmostIdleChanged(PageNodeImpl* page_node) = 0;
  virtual void OnFaviconUpdated(PageNodeImpl* page_node) = 0;
  virtual void OnTitleUpdated(PageNodeImpl* page_node) = 0;
  virtual void OnMainFrameNavigationCommitted(PageNodeImpl* page_node) = 0;

  // ProcessNodeImpl notifications.
  virtual void OnExpectedTaskQueueingDurationSample(
      ProcessNodeImpl* process_node) = 0;
  virtual void OnMainThreadTaskLoadIsLow(ProcessNodeImpl* process_node) = 0;
  virtual void OnRendererIsBloated(ProcessNodeImpl* process_node) = 0;
  virtual void OnAllFramesInProcessFrozen(ProcessNodeImpl* process_node) = 0;

  // SystemNodeImpl notifications.
  virtual void OnProcessCPUUsageReady(SystemNodeImpl* system_node) = 0;

  virtual void SetNodeGraph(GraphImpl* graph) = 0;
};

// An empty implementation of the interface.
class GraphObserverDefaultImpl : public GraphObserver {
 public:
  GraphObserverDefaultImpl();
  ~GraphObserverDefaultImpl() override;

  // Invoked when an observer is added to or removed from the graph. This is a
  // convenient place for observers to initialize any necessary state, validate
  // graph invariants, etc.
  void OnRegistered() override {}
  void OnUnregistered() override {}

  // Called whenever a node has been added to the graph.
  void OnNodeAdded(NodeBase* node) override {}

  // Called when the |node| is about to be removed from the graph.
  void OnBeforeNodeRemoved(NodeBase* node) override {}

  // FrameNodeImpl notifications.
  void OnIsCurrentChanged(FrameNodeImpl* frame_node) override {}
  void OnNetworkAlmostIdleChanged(FrameNodeImpl* frame_node) override {}
  void OnLifecycleStateChanged(FrameNodeImpl* frame_node) override {}
  void OnNonPersistentNotificationCreated(FrameNodeImpl* frame_node) override {}

  // PageNodeImpl notifications.
  void OnIsVisibleChanged(PageNodeImpl* page_node) override {}
  void OnIsLoadingChanged(PageNodeImpl* page_node) override {}
  void OnUkmSourceIdChanged(PageNodeImpl* page_node) override {}
  void OnLifecycleStateChanged(PageNodeImpl* page_node) override {}
  void OnPageAlmostIdleChanged(PageNodeImpl* page_node) override {}
  void OnFaviconUpdated(PageNodeImpl* page_node) override {}
  void OnTitleUpdated(PageNodeImpl* page_node) override {}
  void OnMainFrameNavigationCommitted(PageNodeImpl* page_node) override {}

  // ProcessNodeImpl notifications.
  void OnExpectedTaskQueueingDurationSample(
      ProcessNodeImpl* process_node) override {}
  void OnMainThreadTaskLoadIsLow(ProcessNodeImpl* process_node) override {}
  void OnRendererIsBloated(ProcessNodeImpl* process_node) override {}
  void OnAllFramesInProcessFrozen(ProcessNodeImpl* process_node) override {}

  // SystemNodeImpl notifications.
  void OnProcessCPUUsageReady(SystemNodeImpl* system_node) override {}

  void SetNodeGraph(GraphImpl* graph) override;

  GraphImpl* graph() const { return node_graph_; }

 private:
  GraphImpl* node_graph_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GraphObserverDefaultImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_GRAPH_OBSERVER_H_
