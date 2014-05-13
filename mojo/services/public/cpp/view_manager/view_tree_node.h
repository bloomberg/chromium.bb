// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_TREE_NODE_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_TREE_NODE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "mojo/services/public/cpp/view_manager/view_manager_types.h"

namespace mojo {
namespace services {
namespace view_manager {

class ViewManager;
class ViewTreeNodeObserver;

// ViewTreeNodes are owned by the ViewManager.
// TODO(beng): Right now, you'll have to implement a ViewTreeNodeObserver to
//             track destruction and NULL any pointers you have.
//             Investigate some kind of smart pointer or weak pointer for these.
class ViewTreeNode {
 public:
  typedef std::vector<ViewTreeNode*> Children;

  static ViewTreeNode* Create(ViewManager* view_manager);

  // Destroys this node and all its children.
  void Destroy();

  // Configuration.
  TransportNodeId id() const { return id_; }

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

  ViewTreeNode* GetChildById(TransportNodeId id);

 protected:
  // This class is subclassed only by test classes that provide a public ctor.
  ViewTreeNode();
  ~ViewTreeNode();

 private:
  friend class ViewTreeNodePrivate;

  explicit ViewTreeNode(ViewManager* manager);

  void LocalDestroy();
  void LocalAddChild(ViewTreeNode* child);
  void LocalRemoveChild(ViewTreeNode* child);

  ViewManager* manager_;
  TransportNodeId id_;
  ViewTreeNode* parent_;
  Children children_;

  ObserverList<ViewTreeNodeObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ViewTreeNode);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_TREE_NODE_H_
