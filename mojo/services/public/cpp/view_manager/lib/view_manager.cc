// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/view_manager.h"

#include "mojo/services/public/cpp/view_manager/lib/view_manager_synchronizer.h"
#include "mojo/services/public/cpp/view_manager/lib/view_tree_node_private.h"
#include "mojo/services/public/cpp/view_manager/view.h"

namespace mojo {
namespace view_manager {

ViewManager::ViewManager(Shell* shell)
    : shell_(shell) {}

ViewManager::~ViewManager() {
  while (!nodes_.empty()) {
    IdToNodeMap::iterator it = nodes_.begin();
    if (synchronizer_->OwnsNode(it->second->id()))
      it->second->Destroy();
    else
      nodes_.erase(it);
  }
  while (!views_.empty()) {
    IdToViewMap::iterator it = views_.begin();
    if (synchronizer_->OwnsView(it->second->id()))
      it->second->Destroy();
    else
      views_.erase(it);
  }
}

void ViewManager::Init() {
  synchronizer_.reset(new ViewManagerSynchronizer(this));
}

ViewTreeNode* ViewManager::GetNodeById(TransportNodeId id) {
  IdToNodeMap::const_iterator it = nodes_.find(id);
  return it != nodes_.end() ? it->second : NULL;
}

View* ViewManager::GetViewById(TransportViewId id) {
  IdToViewMap::const_iterator it = views_.find(id);
  return it != views_.end() ? it->second : NULL;
}

}  // namespace view_manager
}  // namespace mojo
