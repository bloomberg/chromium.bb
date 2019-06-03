// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PAGE_NODE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PAGE_NODE_H_

#include "base/macros.h"

namespace performance_manager {

class Graph;
class PageNodeObserver;

// A PageNode represents the root of a FrameTree, or equivalently a WebContents.
// These may correspond to normal tabs, WebViews, Portals, Chrome Apps or
// Extensions.
class PageNode {
 public:
  using Observer = PageNodeObserver;
  class ObserverDefaultImpl;

  PageNode();
  virtual ~PageNode();

  // Returns the graph to which this node belongs.
  virtual Graph* GetGraph() const = 0;

  // Returns the private key which is used for indexing this object in the
  // graph. This is an opaque pointer strictly used for implementation.
  virtual const void* GetIndexingKey() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageNode);
};

// Pure virtual observer interface. Derive from this if you want to be forced to
// implement the entire interface.
class PageNodeObserver {
 public:
  PageNodeObserver();
  virtual ~PageNodeObserver();

  // Node lifetime notifications.

  // Called when a |page_node| is added to the graph.
  virtual void OnPageNodeAdded(const PageNode* page_node) = 0;

  // Called before a |page_node| is removed from the graph.
  virtual void OnBeforePageNodeRemoved(const PageNode* page_node) = 0;

  // Notifications of property changes.

  // Invoked when the |is_visible| property changes.
  virtual void OnIsVisibleChanged(const PageNode* page_node) = 0;

  // Invoked when the |is_loading| property changes.
  virtual void OnIsLoadingChanged(const PageNode* page_node) = 0;

  // Invoked when the |ukm_source_id| property changes.
  virtual void OnUkmSourceIdChanged(const PageNode* page_node) = 0;

  // Invoked when the |lifecycle_state| property changes.
  virtual void OnLifecycleStateChanged(const PageNode* page_node) = 0;

  // Invoked when the |page_almost_idle| property changes.
  virtual void OnPageAlmostIdleChanged(const PageNode* page_node) = 0;

  // This is fired when a main frame navigation commits. It indicates that the
  // |navigation_id| and |main_frame_url| properties have changed.
  virtual void OnMainFrameNavigationCommitted(const PageNode* page_node) = 0;

  // Events with no property changes.

  // Fired when the tab title associated with a page changes. This property is
  // not directly reflected on the node.
  virtual void OnTitleUpdated(const PageNode* page_node) = 0;

  // Fired when the favicon associated with a page is updated. This property is
  // not directly reflected on the node.
  virtual void OnFaviconUpdated(const PageNode* page_node) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageNodeObserver);
};

// Default implementation of observer that provides dummy versions of each
// function. Derive from this if you only need to implement a few of the
// functions.
class PageNode::ObserverDefaultImpl : public PageNodeObserver {
 public:
  ObserverDefaultImpl();
  ~ObserverDefaultImpl() override;

  // PageNodeObserver implementation:
  void OnPageNodeAdded(const PageNode* page_node) override {}
  void OnBeforePageNodeRemoved(const PageNode* page_node) override {}
  void OnIsVisibleChanged(const PageNode* page_node) override {}
  void OnIsLoadingChanged(const PageNode* page_node) override {}
  void OnUkmSourceIdChanged(const PageNode* page_node) override {}
  void OnLifecycleStateChanged(const PageNode* page_node) override {}
  void OnPageAlmostIdleChanged(const PageNode* page_node) override {}
  void OnMainFrameNavigationCommitted(const PageNode* page_node) override {}
  void OnTitleUpdated(const PageNode* page_node) override {}
  void OnFaviconUpdated(const PageNode* page_node) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ObserverDefaultImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PAGE_NODE_H_
