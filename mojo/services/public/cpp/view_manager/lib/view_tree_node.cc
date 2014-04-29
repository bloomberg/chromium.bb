// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/view_tree_node.h"

#include "mojo/services/public/cpp/view_manager/lib/view_manager_private.h"
#include "mojo/services/public/cpp/view_manager/lib/view_manager_synchronizer.h"
#include "mojo/services/public/cpp/view_manager/lib/view_tree_node_private.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node_observer.h"

namespace mojo {
namespace services {
namespace view_manager {

void NotifyViewTreeChangeAtReceiver(
    ViewTreeNode* receiver,
    const ViewTreeNodeObserver::TreeChangeParams& params) {
  ViewTreeNodeObserver::TreeChangeParams local_params = params;
  local_params.receiver = receiver;
  FOR_EACH_OBSERVER(ViewTreeNodeObserver,
                    *ViewTreeNodePrivate(receiver).observers(),
                    OnTreeChange(local_params));
}

void NotifyViewTreeChangeUp(
    ViewTreeNode* start_at,
    const ViewTreeNodeObserver::TreeChangeParams& params) {
  for (ViewTreeNode* current = start_at; current; current = current->parent())
    NotifyViewTreeChangeAtReceiver(current, params);
}

void NotifyViewTreeChangeDown(
    ViewTreeNode* start_at,
    const ViewTreeNodeObserver::TreeChangeParams& params) {
  NotifyViewTreeChangeAtReceiver(start_at, params);
  ViewTreeNode::Children::const_iterator it = start_at->children().begin();
  for (; it != start_at->children().end(); ++it)
    NotifyViewTreeChangeDown(*it, params);
}

void NotifyViewTreeChange(
    const ViewTreeNodeObserver::TreeChangeParams& params) {
  NotifyViewTreeChangeDown(params.target, params);
  switch (params.phase) {
  case ViewTreeNodeObserver::DISPOSITION_CHANGING:
    if (params.old_parent)
      NotifyViewTreeChangeUp(params.old_parent, params);
    break;
  case ViewTreeNodeObserver::DISPOSITION_CHANGED:
    if (params.new_parent)
      NotifyViewTreeChangeUp(params.new_parent, params);
    break;
  default:
    NOTREACHED();
    break;
  }
}

class ScopedTreeNotifier {
 public:
  ScopedTreeNotifier(ViewTreeNode* target,
                     ViewTreeNode* old_parent,
                     ViewTreeNode* new_parent) {
    params_.target = target;
    params_.old_parent = old_parent;
    params_.new_parent = new_parent;
    NotifyViewTreeChange(params_);
  }
  ~ScopedTreeNotifier() {
    params_.phase = ViewTreeNodeObserver::DISPOSITION_CHANGED;
    NotifyViewTreeChange(params_);
  }

 private:
  ViewTreeNodeObserver::TreeChangeParams params_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTreeNotifier);
};

void RemoveChildImpl(ViewTreeNode* child, ViewTreeNode::Children* children) {
  ViewTreeNode::Children::iterator it =
      std::find(children->begin(), children->end(), child);
  if (it != children->end()) {
    children->erase(it);
    ViewTreeNodePrivate(child).ClearParent();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ViewTreeNode, public:

ViewTreeNode::ViewTreeNode()
    : manager_(NULL),
      id_(-1),
      owned_by_parent_(true),
      parent_(NULL) {}

ViewTreeNode::ViewTreeNode(ViewManager* manager)
    : manager_(manager),
      id_(ViewManagerPrivate(manager).synchronizer()->CreateViewTreeNode()),
      owned_by_parent_(true),
      parent_(NULL) {}

ViewTreeNode::~ViewTreeNode() {
  while (!children_.empty()) {
    ViewTreeNode* child = children_.front();
    if (child->owned_by_parent_) {
      delete child;
      // Deleting the child also removes it from our child list.
      DCHECK(std::find(children_.begin(), children_.end(), child) ==
             children_.end());
    } else {
      RemoveChild(child);
    }
  }

  if (parent_)
    parent_->RemoveChild(this);

}

void ViewTreeNode::AddObserver(ViewTreeNodeObserver* observer) {
  observers_.AddObserver(observer);
}

void ViewTreeNode::RemoveObserver(ViewTreeNodeObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ViewTreeNode::AddChild(ViewTreeNode* child) {
  ScopedTreeNotifier notifier(child, child->parent(), this);
  if (child->parent())
    RemoveChildImpl(child, &child->parent_->children_);
  children_.push_back(child);
  child->parent_ = this;
  ViewManagerPrivate(manager_).synchronizer()->AddChild(child->id(), id_);
}

void ViewTreeNode::RemoveChild(ViewTreeNode* child) {
  DCHECK_EQ(this, child->parent());
  ScopedTreeNotifier(child, this, NULL);
  RemoveChildImpl(child, &children_);
  ViewManagerPrivate(manager_).synchronizer()->RemoveChild(child->id(), id_);
}

bool ViewTreeNode::Contains(ViewTreeNode* child) const {
  for (ViewTreeNode* p = child->parent(); p; p = p->parent()) {
    if (p == this)
      return true;
  }
  return false;
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

