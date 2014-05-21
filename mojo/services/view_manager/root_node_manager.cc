// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/root_node_manager.h"

#include "base/logging.h"
#include "mojo/services/view_manager/view_manager_connection.h"
#include "ui/aura/env.h"

namespace mojo {
namespace view_manager {
namespace service {

RootNodeManager::ScopedChange::ScopedChange(
    ViewManagerConnection* connection,
    RootNodeManager* root,
    RootNodeManager::ChangeType change_type,
    bool is_delete_node)
    : root_(root),
      change_type_(change_type) {
  root_->PrepareForChange(connection, is_delete_node);
}

RootNodeManager::ScopedChange::~ScopedChange() {
  root_->FinishChange(change_type_);
}

RootNodeManager::Context::Context() {
  // Pass in false as native viewport creates the PlatformEventSource.
  aura::Env::CreateInstance(false);
}

RootNodeManager::Context::~Context() {
}

RootNodeManager::RootNodeManager(Shell* shell)
    : next_connection_id_(1),
      next_server_change_id_(1),
      change_source_(kRootConnection),
      is_processing_delete_node_(false),
      root_view_manager_(shell, this),
      root_(this, RootNodeId()) {
}

RootNodeManager::~RootNodeManager() {
  // All the connections should have been destroyed.
  DCHECK(connection_map_.empty());
}

TransportConnectionId RootNodeManager::GetAndAdvanceNextConnectionId() {
  const TransportConnectionId id = next_connection_id_++;
  DCHECK_LT(id, next_connection_id_);
  return id;
}

void RootNodeManager::AddConnection(ViewManagerConnection* connection) {
  DCHECK_EQ(0u, connection_map_.count(connection->id()));
  connection_map_[connection->id()] = connection;
}

void RootNodeManager::RemoveConnection(ViewManagerConnection* connection) {
  connection_map_.erase(connection->id());
}

ViewManagerConnection* RootNodeManager::GetConnection(
    TransportConnectionId connection_id) {
  ConnectionMap::iterator i = connection_map_.find(connection_id);
  return i == connection_map_.end() ? NULL : i->second;
}

Node* RootNodeManager::GetNode(const NodeId& id) {
  if (id == root_.id())
    return &root_;
  ConnectionMap::iterator i = connection_map_.find(id.connection_id);
  return i == connection_map_.end() ? NULL : i->second->GetNode(id);
}

View* RootNodeManager::GetView(const ViewId& id) {
  ConnectionMap::iterator i = connection_map_.find(id.connection_id);
  return i == connection_map_.end() ? NULL : i->second->GetView(id);
}

void RootNodeManager::ProcessNodeHierarchyChanged(const Node* node,
                                                  const Node* new_parent,
                                                  const Node* old_parent) {
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end(); ++i) {
    i->second->ProcessNodeHierarchyChanged(
        node, new_parent, old_parent, next_server_change_id_,
        IsChangeSource(i->first));
  }
}

void RootNodeManager::ProcessNodeViewReplaced(const Node* node,
                                              const View* new_view,
                                              const View* old_view) {
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end(); ++i) {
    i->second->ProcessNodeViewReplaced(node, new_view, old_view,
                                       IsChangeSource(i->first));
  }
}

void RootNodeManager::ProcessNodeDeleted(const NodeId& node) {
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end(); ++i) {
    i->second->ProcessNodeDeleted(node, next_server_change_id_,
                                 IsChangeSource(i->first));
  }
}

void RootNodeManager::ProcessViewDeleted(const ViewId& view) {
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end(); ++i) {
    i->second->ProcessViewDeleted(view, IsChangeSource(i->first));
  }
}

void RootNodeManager::PrepareForChange(ViewManagerConnection* connection,
                                       bool is_delete_node) {
  // Should only ever have one change in flight.
  DCHECK_EQ(kRootConnection, change_source_);
  change_source_ = connection->id();
  is_processing_delete_node_ = is_delete_node;
}

void RootNodeManager::FinishChange(ChangeType change_type) {
  // PrepareForChange/FinishChange should be balanced.
  DCHECK_NE(kRootConnection, change_source_);
  change_source_ = 0;
  is_processing_delete_node_ = false;
  if (change_type == CHANGE_TYPE_ADVANCE_SERVER_CHANGE_ID)
    next_server_change_id_++;
}

void RootNodeManager::OnNodeHierarchyChanged(const Node* node,
                                             const Node* new_parent,
                                             const Node* old_parent) {
  if (!root_view_manager_.in_setup())
    ProcessNodeHierarchyChanged(node, new_parent, old_parent);
}

void RootNodeManager::OnNodeViewReplaced(const Node* node,
                                         const View* new_view,
                                         const View* old_view) {
  ProcessNodeViewReplaced(node, new_view, old_view);
}

}  // namespace service
}  // namespace view_manager
}  // namespace mojo
