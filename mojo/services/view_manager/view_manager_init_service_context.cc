// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view_manager_init_service_context.h"

#include "base/bind.h"
#include "mojo/services/view_manager/root_node_manager.h"
#include "mojo/services/view_manager/view_manager_init_service_impl.h"

namespace mojo {
namespace service {

ViewManagerInitServiceContext::ViewManagerInitServiceContext()
    : is_tree_host_ready_(false) {}
ViewManagerInitServiceContext::~ViewManagerInitServiceContext() {}

void ViewManagerInitServiceContext::AddConnection(
    ViewManagerInitServiceImpl* connection) {
  DCHECK(std::find(connections_.begin(), connections_.end(), connection) ==
                   connections_.end());
  connections_.push_back(connection);
}

void ViewManagerInitServiceContext::RemoveConnection(
    ViewManagerInitServiceImpl* connection) {
  Connections::iterator it =
      std::find(connections_.begin(), connections_.end(), connection);
  DCHECK(it != connections_.end());
  connections_.erase(it);

  // This object is owned by an object that outlives the current thread's
  // message loop, so we need to destroy the RootNodeManager earlier, as it may
  // attempt to post tasks during its destruction.
  if (connections_.empty())
    root_node_manager_.reset();
}

void ViewManagerInitServiceContext::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  if (!root_node_manager_.get()) {
    root_node_manager_.reset(new RootNodeManager(
        connection,
        this,
        base::Bind(&ViewManagerInitServiceContext::OnNativeViewportDeleted,
                   base::Unretained(this))));
  }
}

void ViewManagerInitServiceContext::OnNativeViewportDeleted() {
  for (Connections::const_iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    (*it)->OnNativeViewportDeleted();
  }
}

void ViewManagerInitServiceContext::OnRootViewManagerWindowTreeHostCreated() {
  DCHECK(!is_tree_host_ready_);
  is_tree_host_ready_ = true;
  for (Connections::const_iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    (*it)->OnRootViewManagerWindowTreeHostCreated();
  }
}

}  // namespace service
}  // namespace mojo
