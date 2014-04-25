// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/root_node_manager.h"

#include "base/logging.h"
#include "mojo/services/view_manager/view_manager_connection.h"

namespace mojo {
namespace services {
namespace view_manager {
namespace {

// Id for the root node.
const uint16_t kRootId = 1;

}  // namespace

RootNodeManager::ScopedChange::ScopedChange(ViewManagerConnection* connection,
                                            RootNodeManager* root,
                                            int32_t change_id)
    : root_(root) {
  root_->PrepareForChange(connection, change_id);
}

RootNodeManager::ScopedChange::~ScopedChange() {
  root_->FinishChange();
}

RootNodeManager::RootNodeManager()
    : next_connection_id_(1),
      root_(this, NodeId(0, kRootId)) {
}

RootNodeManager::~RootNodeManager() {
}

uint16_t RootNodeManager::GetAndAdvanceNextConnectionId() {
  const uint16_t id = next_connection_id_++;
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

ViewManagerConnection* RootNodeManager::GetConnection(uint16_t connection_id) {
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

void RootNodeManager::NotifyNodeHierarchyChanged(const NodeId& node,
                                                 const NodeId& new_parent,
                                                 const NodeId& old_parent) {
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end(); ++i) {
    const int32_t change_id = (change_ && i->first == change_->connection_id) ?
        change_->change_id : 0;
    i->second->NotifyNodeHierarchyChanged(
        node, new_parent, old_parent, change_id);
  }
}

void RootNodeManager::NotifyNodeViewReplaced(const NodeId& node,
                                            const ViewId& new_view_id,
                                            const ViewId& old_view_id) {
  // TODO(sky): make a macro for this.
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end(); ++i) {
    const int32_t change_id = (change_ && i->first == change_->connection_id) ?
        change_->change_id : 0;
    i->second->NotifyNodeViewReplaced(node, new_view_id, old_view_id,
                                      change_id);
  }
}

void RootNodeManager::PrepareForChange(ViewManagerConnection* connection,
                                       int32_t change_id) {
  DCHECK(!change_.get());  // Should only ever have one change in flight.
  change_.reset(new Change(connection->id(), change_id));
}

void RootNodeManager::FinishChange() {
  DCHECK(change_.get());  // PrepareForChange/FinishChange should be balanced.
  change_.reset();
}

void RootNodeManager::OnCreated() {
}

void RootNodeManager::OnDestroyed() {
}

void RootNodeManager::OnBoundsChanged(const Rect& bounds) {
}

void RootNodeManager::OnEvent(const Event& event,
                              const mojo::Callback<void()>& callback) {
  callback.Run();
}

void RootNodeManager::OnNodeHierarchyChanged(const NodeId& node,
                                             const NodeId& new_parent,
                                             const NodeId& old_parent) {
  NotifyNodeHierarchyChanged(node, new_parent, old_parent);
}

void RootNodeManager::OnNodeViewReplaced(const NodeId& node,
                                         const ViewId& new_view_id,
                                         const ViewId& old_view_id) {
  NotifyNodeViewReplaced(node, new_view_id, old_view_id);
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
