// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view_manager_connection.h"

#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/services/view_manager/node.h"
#include "mojo/services/view_manager/root_node_manager.h"
#include "mojo/services/view_manager/type_converters.h"
#include "mojo/services/view_manager/view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"
#include "ui/gfx/codec/png_codec.h"

namespace mojo {
namespace view_manager {
namespace service {
namespace {

// Places |node| in |nodes| and recurses through the children.
void GetDescendants(Node* node, std::vector<Node*>* nodes) {
  if (!node)
    return;

  nodes->push_back(node);

  std::vector<Node*> children(node->GetChildren());
  for (size_t i = 0 ; i < children.size(); ++i)
    GetDescendants(children[i], nodes);
}

}  // namespace

ViewManagerConnection::ViewManagerConnection(RootNodeManager* root_node_manager)
    : root_node_manager_(root_node_manager),
      id_(0) {
}

ViewManagerConnection::~ViewManagerConnection() {
  // Delete any views we own.
  while (!view_map_.empty()) {
    bool result = DeleteViewImpl(this, view_map_.begin()->second->id());
    DCHECK(result);
  }

  // We're about to destroy all our nodes. Detach any views from them.
  for (NodeMap::iterator i = node_map_.begin(); i != node_map_.end(); ++i) {
    if (i->second->view()) {
      bool result = SetViewImpl(i->second->id(), ViewId());
      DCHECK(result);
    }
  }

  STLDeleteContainerPairSecondPointers(node_map_.begin(), node_map_.end());
  root_node_manager_->RemoveConnection(this);
}

void ViewManagerConnection::OnConnectionEstablished() {
  DCHECK_EQ(0, id_);  // Should only get OnConnectionEstablished() once.
  id_ = root_node_manager_->GetAndAdvanceNextConnectionId();
  root_node_manager_->AddConnection(this);
  std::vector<Node*> to_send;
  GetUnknownNodesFrom(root_node_manager_->root(), &to_send);
  AllocationScope allocation_scope;
  client()->OnConnectionEstablished(
      id_,
      root_node_manager_->next_server_change_id(),
      Array<INode>::From(to_send));
}

void ViewManagerConnection::OnConnectionError() {}

Node* ViewManagerConnection::GetNode(const NodeId& id) {
  if (id_ == id.connection_id) {
    NodeMap::iterator i = node_map_.find(id.node_id);
    return i == node_map_.end() ? NULL : i->second;
  }
  return root_node_manager_->GetNode(id);
}

View* ViewManagerConnection::GetView(const ViewId& id) {
  if (id_ == id.connection_id) {
    ViewMap::const_iterator i = view_map_.find(id.view_id);
    return i == view_map_.end() ? NULL : i->second;
  }
  return root_node_manager_->GetView(id);
}

void ViewManagerConnection::ProcessNodeHierarchyChanged(
    const NodeId& node_id,
    const NodeId& new_parent_id,
    const NodeId& old_parent_id,
    TransportChangeId server_change_id,
    bool originated_change) {
  if (originated_change || root_node_manager_->is_processing_delete_node())
    return;
  std::vector<Node*> to_send;
  if (!ShouldNotifyOnHierarchyChange(node_id, new_parent_id, old_parent_id,
                                     &to_send)) {
    if (root_node_manager_->IsProcessingChange()) {
      client()->OnServerChangeIdAdvanced(
          root_node_manager_->next_server_change_id() + 1);
    }
    return;
  }
  AllocationScope allocation_scope;
  client()->OnNodeHierarchyChanged(NodeIdToTransportId(node_id),
                                   NodeIdToTransportId(new_parent_id),
                                   NodeIdToTransportId(old_parent_id),
                                   server_change_id,
                                   Array<INode>::From(to_send));
}

void ViewManagerConnection::ProcessNodeViewReplaced(
    const NodeId& node,
    const ViewId& new_view_id,
    const ViewId& old_view_id,
    bool originated_change) {
  if (originated_change)
    return;
  client()->OnNodeViewReplaced(NodeIdToTransportId(node),
                               ViewIdToTransportId(new_view_id),
                               ViewIdToTransportId(old_view_id));
}

void ViewManagerConnection::ProcessNodeDeleted(
    const NodeId& node,
    TransportChangeId server_change_id,
    bool originated_change) {
  const bool in_known = known_nodes_.erase(NodeIdToTransportId(node)) > 0;

  if (originated_change)
    return;

  if (in_known) {
    client()->OnNodeDeleted(NodeIdToTransportId(node), server_change_id);
  } else if (root_node_manager_->IsProcessingChange()) {
    client()->OnServerChangeIdAdvanced(
        root_node_manager_->next_server_change_id() + 1);
  }
}

void ViewManagerConnection::ProcessViewDeleted(const ViewId& view,
                                               bool originated_change) {
  if (originated_change)
    return;
  client()->OnViewDeleted(ViewIdToTransportId(view));
}

bool ViewManagerConnection::DeleteNodeImpl(ViewManagerConnection* source,
                                           const NodeId& node_id) {
  DCHECK_EQ(node_id.connection_id, id_);
  Node* node = GetNode(node_id);
  if (!node)
    return false;
  RootNodeManager::ScopedChange change(
      source, root_node_manager_,
      RootNodeManager::CHANGE_TYPE_ADVANCE_SERVER_CHANGE_ID, true);
  if (node->GetParent())
    node->GetParent()->Remove(node);
  std::vector<Node*> children(node->GetChildren());
  for (size_t i = 0; i < children.size(); ++i)
    node->Remove(children[i]);
  DCHECK(node->GetChildren().empty());
  node_map_.erase(node_id.node_id);
  delete node;
  node = NULL;
  root_node_manager_->ProcessNodeDeleted(node_id);
  return true;
}

bool ViewManagerConnection::DeleteViewImpl(ViewManagerConnection* source,
                                           const ViewId& view_id) {
  DCHECK_EQ(view_id.connection_id, id_);
  View* view = GetView(view_id);
  if (!view)
    return false;
  RootNodeManager::ScopedChange change(
      source, root_node_manager_,
      RootNodeManager::CHANGE_TYPE_DONT_ADVANCE_SERVER_CHANGE_ID, false);
  if (view->node())
    view->node()->SetView(NULL);
  view_map_.erase(view_id.view_id);
  // Make a copy of |view_id| as once we delete view |view_id| may no longer be
  // valid.
  const ViewId view_id_copy(view_id);
  delete view;
  root_node_manager_->ProcessViewDeleted(view_id_copy);
  return true;
}

bool ViewManagerConnection::SetViewImpl(const NodeId& node_id,
                                        const ViewId& view_id) {
  Node* node = GetNode(node_id);
  if (!node)
    return false;
  View* view = GetView(view_id);
  if (!view && view_id != ViewId())
    return false;
  RootNodeManager::ScopedChange change(
      this, root_node_manager_,
      RootNodeManager::CHANGE_TYPE_DONT_ADVANCE_SERVER_CHANGE_ID, false);
  node->SetView(view);
  return true;
}

void ViewManagerConnection::GetUnknownNodesFrom(
    Node* node,
    std::vector<Node*>* nodes) {
  const TransportNodeId transport_id = NodeIdToTransportId(node->id());
  if (known_nodes_.count(transport_id) == 1)
    return;
  nodes->push_back(node);
  known_nodes_.insert(transport_id);
  std::vector<Node*> children(node->GetChildren());
  for (size_t i = 0 ; i < children.size(); ++i)
    GetUnknownNodesFrom(children[i], nodes);
}

bool ViewManagerConnection::ShouldNotifyOnHierarchyChange(
    const NodeId& node_id,
    const NodeId& new_parent_id,
    const NodeId& old_parent_id,
    std::vector<Node*>* to_send) {
  Node* new_parent = GetNode(new_parent_id);
  if (new_parent) {
    // On getting a new parent we may need to communicate new nodes to the
    // client. We do that in the following cases:
    // . New parent is a descendant of the root. In this case the client already
    //   knows all ancestors, so we only have to communicate descendants of node
    //   the client doesn't know about.
    // . If the client knew about the parent, we have to do the same.
    // . If the client knows about the node and is added to a tree the client
    //   doesn't know about we have to communicate from the root down (the
    //   client is learning about a new root).
    if (root_node_manager_->root()->Contains(new_parent) ||
        known_nodes_.count(NodeIdToTransportId(new_parent_id))) {
      GetUnknownNodesFrom(GetNode(node_id), to_send);
      return true;
    }
    // If parent wasn't known we have to communicate from the root down.
    if (known_nodes_.count(NodeIdToTransportId(node_id))) {
      GetUnknownNodesFrom(new_parent->GetRoot(), to_send);
      return true;
    }
  }
  // Otherwise only communicate the change if the node was known. We shouldn't
  // need to communicate any nodes on a remove.
  return known_nodes_.count(NodeIdToTransportId(node_id)) > 0;
}

void ViewManagerConnection::CreateNode(
    TransportNodeId transport_node_id,
    const Callback<void(bool)>& callback) {
  const NodeId node_id(NodeIdFromTransportId(transport_node_id));
  if (node_id.connection_id != id_ ||
      node_map_.find(node_id.node_id) != node_map_.end()) {
    callback.Run(false);
    return;
  }
  node_map_[node_id.node_id] = new Node(this, node_id);
  callback.Run(true);
}

void ViewManagerConnection::DeleteNode(
    TransportNodeId transport_node_id,
    const Callback<void(bool)>& callback) {
  AllocationScope allocation_scope;
  const NodeId node_id(NodeIdFromTransportId(transport_node_id));
  ViewManagerConnection* connection = root_node_manager_->GetConnection(
      node_id.connection_id);
  callback.Run(connection &&
               connection->DeleteNodeImpl(this, node_id));
}

void ViewManagerConnection::AddNode(
    TransportNodeId parent_id,
    TransportNodeId child_id,
    TransportChangeId server_change_id,
    const Callback<void(bool)>& callback) {
  bool success = false;
  if (server_change_id == root_node_manager_->next_server_change_id()) {
    Node* parent = GetNode(NodeIdFromTransportId(parent_id));
    Node* child = GetNode(NodeIdFromTransportId(child_id));
    if (parent && child && child->GetParent() != parent &&
        !child->window()->Contains(parent->window())) {
      success = true;
      RootNodeManager::ScopedChange change(
          this, root_node_manager_,
          RootNodeManager::CHANGE_TYPE_ADVANCE_SERVER_CHANGE_ID, false);
      parent->Add(child);
    }
  }
  callback.Run(success);
}

void ViewManagerConnection::RemoveNodeFromParent(
    TransportNodeId node_id,
    TransportChangeId server_change_id,
    const Callback<void(bool)>& callback) {
  bool success = false;
  if (server_change_id == root_node_manager_->next_server_change_id()) {
    Node* node = GetNode(NodeIdFromTransportId(node_id));
    if (node && node->GetParent()) {
      success = true;
      RootNodeManager::ScopedChange change(
          this, root_node_manager_,
          RootNodeManager::CHANGE_TYPE_ADVANCE_SERVER_CHANGE_ID, false);
      node->GetParent()->Remove(node);
    }
  }
  callback.Run(success);
}

void ViewManagerConnection::GetNodeTree(
    TransportNodeId node_id,
    const Callback<void(Array<INode>)>& callback) {
  AllocationScope allocation_scope;
  Node* node = GetNode(NodeIdFromTransportId(node_id));
  std::vector<Node*> nodes;
  GetDescendants(node, &nodes);
  for (size_t i = 0; i < nodes.size(); ++i)
    known_nodes_.insert(NodeIdToTransportId(nodes[i]->id()));
  callback.Run(Array<INode>::From(nodes));
}

void ViewManagerConnection::CreateView(
    TransportViewId transport_view_id,
    const Callback<void(bool)>& callback) {
  const ViewId view_id(ViewIdFromTransportId(transport_view_id));
  if (view_id.connection_id != id_ || view_map_.count(view_id.view_id)) {
    callback.Run(false);
    return;
  }
  view_map_[view_id.view_id] = new View(view_id);
  callback.Run(true);
}

void ViewManagerConnection::DeleteView(
    TransportViewId transport_view_id,
    const Callback<void(bool)>& callback) {
  const ViewId view_id(ViewIdFromTransportId(transport_view_id));
  ViewManagerConnection* connection = root_node_manager_->GetConnection(
      view_id.connection_id);
  callback.Run(connection && connection->DeleteViewImpl(this, view_id));
}

void ViewManagerConnection::SetView(
    TransportNodeId transport_node_id,
    TransportViewId transport_view_id,
    const Callback<void(bool)>& callback) {
  const NodeId node_id(NodeIdFromTransportId(transport_node_id));
  callback.Run(SetViewImpl(node_id, ViewIdFromTransportId(transport_view_id)));
}

void ViewManagerConnection::SetViewContents(
    TransportViewId view_id,
    ScopedSharedBufferHandle buffer,
    uint32_t buffer_size) {
  View* view = GetView(ViewIdFromTransportId(view_id));
  if (!view)
    return;
  void* handle_data;
  if (MapBuffer(buffer.get(), 0, buffer_size, &handle_data,
                MOJO_MAP_BUFFER_FLAG_NONE) != MOJO_RESULT_OK) {
    return;
  }
  SkBitmap bitmap;
  gfx::PNGCodec::Decode(static_cast<const unsigned char*>(handle_data),
                        buffer_size, &bitmap);
  view->SetBitmap(bitmap);
  UnmapBuffer(handle_data);
}

void ViewManagerConnection::OnNodeHierarchyChanged(const NodeId& node,
                                                   const NodeId& new_parent,
                                                   const NodeId& old_parent) {
  root_node_manager_->ProcessNodeHierarchyChanged(node, new_parent, old_parent);
}

void ViewManagerConnection::OnNodeViewReplaced(const NodeId& node,
                                               const ViewId& new_view_id,
                                               const ViewId& old_view_id) {
  root_node_manager_->ProcessNodeViewReplaced(node, new_view_id, old_view_id);
}

}  // namespace service
}  // namespace view_manager
}  // namespace mojo
