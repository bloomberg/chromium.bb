// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view_manager_connection.h"

#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/services/view_manager/node.h"
#include "mojo/services/view_manager/root_node_manager.h"
#include "mojo/services/view_manager/view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"
#include "ui/gfx/codec/png_codec.h"

namespace mojo {
namespace view_manager {
namespace service {
namespace {

// Implementation of NodeCount(). |count| is the current count.
void NodeCountImpl(Node* node, size_t* count) {
  (*count)++;
  std::vector<Node*> children(node->GetChildren());
  for (size_t i = 0 ; i < children.size(); ++i)
    NodeCountImpl(children[i], count);
}

// Returns the number of descendants of |node|.
size_t NodeCount(Node* node) {
  size_t count = 0;
  if (node)
    NodeCountImpl(node, &count);
  return count;
}

// Converts a Node to an INode, putting the result at |index| in
// |array_builder|. This then recurses through the children.
void NodeToINode(Node* node,
                 Array<INode>::Builder* array_builder,
                 size_t* index) {
  if (!node)
    return;

  INode::Builder builder;
  Node* parent = node->GetParent();
  builder.set_parent_id(NodeIdToTransportId(parent ? parent->id() : NodeId()));
  builder.set_node_id(NodeIdToTransportId(node->id()));
  builder.set_view_id(ViewIdToTransportId(
                          node->view() ? node->view()->id() : ViewId()));
  (*array_builder)[*index] = builder.Finish();

  std::vector<Node*> children(node->GetChildren());
  for (size_t i = 0 ; i < children.size(); ++i) {
    (*index)++;
    NodeToINode(children[i], array_builder, index);
  }
}

}  // namespace

ViewManagerConnection::ViewManagerConnection() : id_(0) {
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
  context()->RemoveConnection(this);
}

void ViewManagerConnection::Initialize() {
  DCHECK_EQ(0, id_);  // Should only get Initialize() once.
  id_ = context()->GetAndAdvanceNextConnectionId();
  context()->AddConnection(this);
  client()->OnConnectionEstablished(id_, context()->next_server_change_id());
}

Node* ViewManagerConnection::GetNode(const NodeId& id) {
  if (id_ == id.connection_id) {
    NodeMap::iterator i = node_map_.find(id.node_id);
    return i == node_map_.end() ? NULL : i->second;
  }
  return context()->GetNode(id);
}

View* ViewManagerConnection::GetView(const ViewId& id) {
  if (id_ == id.connection_id) {
    ViewMap::const_iterator i = view_map_.find(id.view_id);
    return i == view_map_.end() ? NULL : i->second;
  }
  return context()->GetView(id);
}

void ViewManagerConnection::NotifyNodeHierarchyChanged(
    const NodeId& node,
    const NodeId& new_parent,
    const NodeId& old_parent,
    TransportChangeId server_change_id) {
  client()->OnNodeHierarchyChanged(NodeIdToTransportId(node),
                                   NodeIdToTransportId(new_parent),
                                   NodeIdToTransportId(old_parent),
                                   server_change_id);
}

void ViewManagerConnection::NotifyNodeViewReplaced(
    const NodeId& node,
    const ViewId& new_view_id,
    const ViewId& old_view_id) {
  client()->OnNodeViewReplaced(NodeIdToTransportId(node),
                               ViewIdToTransportId(new_view_id),
                               ViewIdToTransportId(old_view_id));
}

void ViewManagerConnection::NotifyNodeDeleted(
    const NodeId& node,
    TransportChangeId server_change_id) {
  client()->OnNodeDeleted(NodeIdToTransportId(node), server_change_id);
}

void ViewManagerConnection::NotifyViewDeleted(const ViewId& view) {
  client()->OnViewDeleted(ViewIdToTransportId(view));
}

bool ViewManagerConnection::DeleteNodeImpl(ViewManagerConnection* source,
                                           const NodeId& node_id) {
  DCHECK_EQ(node_id.connection_id, id_);
  Node* node = GetNode(node_id);
  if (!node)
    return false;
  RootNodeManager::ScopedChange change(
      source, context(), RootNodeManager::CHANGE_TYPE_ADVANCE_SERVER_CHANGE_ID);
  if (node->GetParent())
    node->GetParent()->Remove(node);
  std::vector<Node*> children(node->GetChildren());
  for (size_t i = 0; i < children.size(); ++i)
    node->Remove(children[i]);
  DCHECK(node->GetChildren().empty());
  node_map_.erase(node_id.node_id);
  delete node;
  node = NULL;
  context()->NotifyNodeDeleted(node_id);
  return true;
}

bool ViewManagerConnection::DeleteViewImpl(ViewManagerConnection* source,
                                           const ViewId& view_id) {
  DCHECK_EQ(view_id.connection_id, id_);
  View* view = GetView(view_id);
  if (!view)
    return false;
  RootNodeManager::ScopedChange change(
      source, context(),
      RootNodeManager::CHANGE_TYPE_DONT_ADVANCE_SERVER_CHANGE_ID);
  if (view->node())
    view->node()->SetView(NULL);
  view_map_.erase(view_id.view_id);
  // Make a copy of |view_id| as once we delete view |view_id| may no longer be
  // valid.
  const ViewId view_id_copy(view_id);
  delete view;
  context()->NotifyViewDeleted(view_id_copy);
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
      this, context(),
      RootNodeManager::CHANGE_TYPE_DONT_ADVANCE_SERVER_CHANGE_ID);
  node->SetView(view);
  return true;
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
  const NodeId node_id(NodeIdFromTransportId(transport_node_id));
  ViewManagerConnection* connection = context()->GetConnection(
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
  if (server_change_id == context()->next_server_change_id()) {
    Node* parent = GetNode(NodeIdFromTransportId(parent_id));
    Node* child = GetNode(NodeIdFromTransportId(child_id));
    if (parent && child && child->GetParent() != parent &&
        !child->window()->Contains(parent->window())) {
      success = true;
      RootNodeManager::ScopedChange change(
          this, context(),
          RootNodeManager::CHANGE_TYPE_ADVANCE_SERVER_CHANGE_ID);
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
  if (server_change_id == context()->next_server_change_id()) {
    Node* node = GetNode(NodeIdFromTransportId(node_id));
    if (node && node->GetParent()) {
      success = true;
      RootNodeManager::ScopedChange change(
          this, context(),
          RootNodeManager::CHANGE_TYPE_ADVANCE_SERVER_CHANGE_ID);
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
  Array<INode>::Builder array_builder(NodeCount(node));
  {
    size_t index = 0;
    NodeToINode(node, &array_builder, &index);
  }
  callback.Run(array_builder.Finish());
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
  ViewManagerConnection* connection = context()->GetConnection(
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
  context()->NotifyNodeHierarchyChanged(node, new_parent, old_parent);
}

void ViewManagerConnection::OnNodeViewReplaced(const NodeId& node,
                                               const ViewId& new_view_id,
                                               const ViewId& old_view_id) {
  context()->NotifyNodeViewReplaced(node, new_view_id, old_view_id);
}

}  // namespace service
}  // namespace view_manager
}  // namespace mojo
