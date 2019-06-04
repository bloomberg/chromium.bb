// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_FRAME_NODE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_FRAME_NODE_H_

#include "base/macros.h"
#include "chrome/browser/performance_manager/public/graph/node.h"

namespace performance_manager {

class FrameNodeObserver;

// Frame nodes form a tree structure, each FrameNode at most has one parent that
// is a FrameNode. Conceptually, a frame corresponds to a
// content::RenderFrameHost in the browser, and a content::RenderFrameImpl /
// blink::LocalFrame/blink::Document in a renderer.
//
// Note that a frame in a frame tree can be replaced with another, with the
// continuity of that position represented via the |frame_tree_node_id|. It is
// possible to have multiple "sibling" nodes that share the same
// |frame_tree_node_id|. Only one of these may contribute to the content being
// rendered, and this node is designated the "current" node in content
// terminology. A swap is effectively atomic but will take place in two steps
// in the graph: the outgoing frame will first be marked as not current, and the
// incoming frame will be marked as current. As such, the graph invariant is
// that there will be 0 or 1 |is_current| frames with a given
// |frame_tree_node_id|.
//
// This occurs when a frame is navigated and the existing frame can't be reused.
// In that case a "provisional" frame is created to start the navigation. Once
// the navigation completes (which may actually involve a redirect to another
// origin meaning the frame has to be destroyed and another one created in
// another process!) and commits, the frame will be swapped with the previously
// active frame.
class FrameNode : public Node {
 public:
  using Observer = FrameNodeObserver;
  class ObserverDefaultImpl;

  FrameNode();
  ~FrameNode() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameNode);
};

// Pure virtual observer interface. Derive from this if you want to be forced to
// implement the entire interface.
class FrameNodeObserver {
 public:
  FrameNodeObserver();
  virtual ~FrameNodeObserver();

  // Node lifetime notifications.

  // Called when a |frame_node| is added to the graph.
  virtual void OnFrameNodeAdded(const FrameNode* frame_node) = 0;

  // Called before a |frame_node| is removed from the graph.
  virtual void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) = 0;

  // Notifications of property changes.

  // Invoked when the |is_current| property changes.
  virtual void OnIsCurrentChanged(const FrameNode* frame_node) = 0;

  // Invoked when the |network_almost_idle| property changes.
  virtual void OnNetworkAlmostIdleChanged(const FrameNode* frame_node) = 0;

  // Invoked when the |lifecycle_state| property changes.
  virtual void OnLifecycleStateChanged(const FrameNode* frame_node) = 0;

  // Invoked when the |url| property changes.
  virtual void OnURLChanged(const FrameNode* frame_node) = 0;

  // Events with no property changes.

  // Invoked when a non-persistent notification has been issued by the frame.
  virtual void OnNonPersistentNotificationCreated(
      const FrameNode* frame_node) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameNodeObserver);
};

// Default implementation of observer that provides dummy versions of each
// function. Derive from this if you only need to implement a few of the
// functions.
class FrameNode::ObserverDefaultImpl : public FrameNodeObserver {
 public:
  ObserverDefaultImpl();
  ~ObserverDefaultImpl() override;

  // FrameNodeObserver implementation:
  void OnFrameNodeAdded(const FrameNode* frame_node) override {}
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override {}
  void OnIsCurrentChanged(const FrameNode* frame_node) override {}
  void OnNetworkAlmostIdleChanged(const FrameNode* frame_node) override {}
  void OnLifecycleStateChanged(const FrameNode* frame_node) override {}
  void OnURLChanged(const FrameNode* frame_node) override {}
  void OnNonPersistentNotificationCreated(
      const FrameNode* frame_node) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ObserverDefaultImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_FRAME_NODE_H_
