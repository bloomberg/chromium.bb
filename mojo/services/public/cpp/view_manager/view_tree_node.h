// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_TREE_NODE_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_TREE_NODE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/observer_list.h"

namespace mojo {
namespace services {
namespace view_manager {

class ViewTreeNodeObserver;

class ViewTreeNode {
 public:
  typedef std::vector<ViewTreeNode*> Children;

  ViewTreeNode();
  ~ViewTreeNode();

  // Configuration.
  void set_owned_by_parent(bool owned_by_parent) {
    owned_by_parent_ = owned_by_parent;
  }

  // Observation.
  void AddObserver(ViewTreeNodeObserver* observer);
  void RemoveObserver(ViewTreeNodeObserver* observer);

  // Tree.
  ViewTreeNode* parent() { return parent_; }
  const ViewTreeNode* parent() const { return parent_; }
  const Children& children() const { return children_; }

  void AddChild(ViewTreeNode* child);
  void RemoveChild(ViewTreeNode* child);

  bool Contains(ViewTreeNode* child) const;

 private:
  friend class ViewTreeNodePrivate;

  bool owned_by_parent_;
  ViewTreeNode* parent_;
  Children children_;

  ObserverList<ViewTreeNodeObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ViewTreeNode);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_TREE_NODE_H_
