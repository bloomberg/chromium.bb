// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/lib/view_manager_private.h"

#include "mojo/services/public/cpp/view_manager/lib/view_tree_node_private.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node_observer.h"

namespace mojo {
namespace view_manager {
namespace {

// Responsible for removing a root from the ViewManager when that node is
// destroyed.
class RootObserver : public ViewTreeNodeObserver {
 public:
  explicit RootObserver(ViewTreeNode* root) : root_(root) {}
  virtual ~RootObserver() {}

 private:
  // Overridden from ViewTreeNodeObserver:
  virtual void OnNodeDestroy(ViewTreeNode* node,
                             DispositionChangePhase phase) OVERRIDE {
    DCHECK_EQ(node, root_);
    if (phase != ViewTreeNodeObserver::DISPOSITION_CHANGED)
      return;
    ViewManagerPrivate(
        ViewTreeNodePrivate(root_).view_manager()).RemoveRoot(root_);
  }

  ViewTreeNode* root_;

  DISALLOW_COPY_AND_ASSIGN(RootObserver);
};

}  // namespace

ViewManagerPrivate::ViewManagerPrivate(ViewManager* manager)
    : manager_(manager) {}
ViewManagerPrivate::~ViewManagerPrivate() {}

void ViewManagerPrivate::AddRoot(ViewTreeNode* root) {
  // A new root must not already exist as a root or be contained by an existing
  // hierarchy visible to this view manager.
  std::vector<ViewTreeNode*>::const_iterator it = manager_->roots().begin();
  for (; it != manager_->roots().end(); ++it) {
    if (*it == root || (*it)->Contains(root))
      return;
  }
  manager_->roots_.push_back(root);
  root->AddObserver(new RootObserver(root));
  if (!manager_->root_added_callback_.is_null())
    manager_->root_added_callback_.Run(manager_, root);
}

void ViewManagerPrivate::RemoveRoot(ViewTreeNode* root) {
  std::vector<ViewTreeNode*>::iterator it =
      std::find(manager_->roots_.begin(), manager_->roots_.end(), root);
  if (it != manager_->roots_.end()) {
    manager_->roots_.erase(it);
    if (!manager_->root_removed_callback_.is_null())
      manager_->root_removed_callback_.Run(manager_, root);
  }
}

void ViewManagerPrivate::AddNode(TransportNodeId node_id, ViewTreeNode* node) {
  DCHECK(!manager_->nodes_[node_id]);
  manager_->nodes_[node_id] = node;
}

void ViewManagerPrivate::RemoveNode(TransportNodeId node_id) {
  ViewManager::IdToNodeMap::iterator it = manager_->nodes_.find(node_id);
  if (it != manager_->nodes_.end())
    manager_->nodes_.erase(it);
}

void ViewManagerPrivate::AddView(TransportViewId view_id, View* view) {
  DCHECK(!manager_->views_[view_id]);
  manager_->views_[view_id] = view;
}

void ViewManagerPrivate::RemoveView(TransportViewId view_id) {
  ViewManager::IdToViewMap::iterator it = manager_->views_.find(view_id);
  if (it != manager_->views_.end())
    manager_->views_.erase(it);
}

}  // namespace view_manager
}  // namespace mojo
