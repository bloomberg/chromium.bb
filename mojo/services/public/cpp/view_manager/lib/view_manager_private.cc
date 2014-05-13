// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/lib/view_manager_private.h"

namespace mojo {
namespace services {
namespace view_manager {

ViewManagerPrivate::ViewManagerPrivate(ViewManager* manager)
    : manager_(manager) {}
ViewManagerPrivate::~ViewManagerPrivate() {}

void ViewManagerPrivate::AddNode(TransportNodeId node_id, ViewTreeNode* node) {
  manager_->nodes_[node_id] = node;
}

void ViewManagerPrivate::RemoveNode(TransportNodeId node_id) {
  ViewManager::IdToNodeMap::iterator it = manager_->nodes_.find(node_id);
  if (it != manager_->nodes_.end())
    manager_->nodes_.erase(it);
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
