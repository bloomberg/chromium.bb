// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view_manager_connection.h"

#include "base/stl_util.h"
#include "mojo/services/view_manager/node.h"
#include "mojo/services/view_manager/root_node_manager.h"

namespace mojo {
namespace services {
namespace view_manager {

ViewManagerConnection::ViewManagerConnection() : id_(0) {
}

ViewManagerConnection::~ViewManagerConnection() {
  STLDeleteContainerPairSecondPointers(node_map_.begin(), node_map_.end());
  context()->RemoveConnection(this);
}

void ViewManagerConnection::Initialize(
    ServiceConnector<ViewManagerConnection, RootNodeManager>* service_factory,
    ScopedMessagePipeHandle client_handle) {
  DCHECK_EQ(0, id_);  // Should only get Initialize() once.
  ServiceConnection<ViewManager, ViewManagerConnection, RootNodeManager>::
      Initialize(service_factory, client_handle.Pass());
  id_ = context()->GetAndAdvanceNextConnectionId();
  context()->AddConnection(this);
  client()->OnConnectionEstablished(id_);
}

Node* ViewManagerConnection::GetNode(const NodeId& id) {
  if (id_ == id.connection_id) {
    NodeMap::iterator i = node_map_.find(id.node_id);
    return i == node_map_.end() ? NULL : i->second;
  }
  return context()->GetNode(id);
}

void ViewManagerConnection::NotifyNodeHierarchyChanged(
    const NodeId& node,
    const NodeId& new_parent,
    const NodeId& old_parent,
    int32_t change_id) {
  client()->OnNodeHierarchyChanged(NodeIdToTransportId(node),
                                   NodeIdToTransportId(new_parent),
                                   NodeIdToTransportId(old_parent),
                                   change_id);
}

void ViewManagerConnection::CreateNode(
    uint16_t node_id,
    const Callback<void(bool)>& callback) {
  // Negative values are reserved.
  if (node_map_.find(node_id) != node_map_.end()) {
    callback.Run(false);
    return;
  }
  node_map_[node_id] = new Node(this, NodeId(id_, node_id));
  callback.Run(true);
}

void ViewManagerConnection::AddNode(
    uint32_t parent_id,
    uint32_t child_id,
    int32_t change_id,
    const Callback<void(bool)>& callback) {
  Node* parent = GetNode(NodeIdFromTransportId(parent_id));
  Node* child = GetNode(NodeIdFromTransportId(child_id));
  const bool success = parent && child && parent != child;
  if (success) {
    RootNodeManager::ScopedChange change(this, context(), change_id);
    parent->Add(child);
  }
  callback.Run(success);
}

void ViewManagerConnection::RemoveNodeFromParent(
      uint32_t node_id,
      int32_t change_id,
      const Callback<void(bool)>& callback) {
  Node* node = GetNode(NodeIdFromTransportId(node_id));
  const bool success = (node && node->GetParent());
  if (success) {
    RootNodeManager::ScopedChange change(this, context(), change_id);
    node->GetParent()->Remove(node);
  }
  callback.Run(success);
}

void ViewManagerConnection::OnNodeHierarchyChanged(const NodeId& node,
                                                   const NodeId& new_parent,
                                                   const NodeId& old_parent) {
  context()->NotifyNodeHierarchyChanged(node, new_parent, old_parent);
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
