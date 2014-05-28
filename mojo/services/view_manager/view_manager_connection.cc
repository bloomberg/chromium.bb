// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view_manager_connection.h"

#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
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

// Places |node| in |nodes| and recurses through the children.
void GetDescendants(const Node* node, std::vector<const Node*>* nodes) {
  if (!node)
    return;

  nodes->push_back(node);

  std::vector<const Node*> children(node->GetChildren());
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
      bool result = SetViewImpl(i->second, ViewId());
      DCHECK(result);
    }
  }

  if (!node_map_.empty()) {
    RootNodeManager::ScopedChange change(
        this, root_node_manager_,
        RootNodeManager::CHANGE_TYPE_ADVANCE_SERVER_CHANGE_ID, true);
    while (!node_map_.empty()) {
      Node* node = node_map_.begin()->second;
      Node* parent = node->GetParent();
      const NodeId node_id(node->id());
      if (parent)
        parent->Remove(node);
      root_node_manager_->ProcessNodeDeleted(node_id);
      node_map_.erase(NodeIdToTransportId(node_id));
    }
  }

  root_node_manager_->RemoveConnection(this);
}

const Node* ViewManagerConnection::GetNode(const NodeId& id) const {
  if (id_ == id.connection_id) {
    NodeMap::const_iterator i = node_map_.find(id.node_id);
    return i == node_map_.end() ? NULL : i->second;
  }
  return root_node_manager_->GetNode(id);
}

const View* ViewManagerConnection::GetView(const ViewId& id) const {
  if (id_ == id.connection_id) {
    ViewMap::const_iterator i = view_map_.find(id.view_id);
    return i == view_map_.end() ? NULL : i->second;
  }
  return root_node_manager_->GetView(id);
}

void ViewManagerConnection::SetRoots(const Array<TransportNodeId>& node_ids) {
  DCHECK_EQ(0, id_);  // Only valid before connection established.
  NodeIdSet roots;
  for (size_t i = 0; i < node_ids.size(); ++i) {
    DCHECK(GetNode(NodeIdFromTransportId(node_ids[i])));
    roots.insert(node_ids[i]);
  }
  roots_.swap(roots);
}

void ViewManagerConnection::ProcessNodeBoundsChanged(
    const Node* node,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    bool originated_change) {
  if (originated_change)
    return;
  TransportNodeId node_id = NodeIdToTransportId(node->id());
  if (known_nodes_.count(node_id) > 0) {
    AllocationScope scope;
    client()->OnNodeBoundsChanged(node_id, old_bounds, new_bounds);
  }
}

void ViewManagerConnection::ProcessNodeHierarchyChanged(
    const Node* node,
    const Node* new_parent,
    const Node* old_parent,
    TransportChangeId server_change_id,
    bool originated_change) {
  if (known_nodes_.count(NodeIdToTransportId(node->id())) > 0) {
    if (originated_change)
      return;
    if (node->id().connection_id != id_ && !IsNodeDescendantOfRoots(node)) {
      // Node was a descendant of roots and is no longer, treat it as though the
      // node was deleted.
      RemoveFromKnown(node);
      client()->OnNodeDeleted(NodeIdToTransportId(node->id()),
                              server_change_id);
      return;
    }
  }

  if (originated_change || root_node_manager_->is_processing_delete_node())
    return;
  std::vector<const Node*> to_send;
  if (!ShouldNotifyOnHierarchyChange(node, &new_parent, &old_parent,
                                     &to_send)) {
    if (root_node_manager_->IsProcessingChange()) {
      client()->OnServerChangeIdAdvanced(
          root_node_manager_->next_server_change_id() + 1);
    }
    return;
  }
  AllocationScope allocation_scope;
  const NodeId new_parent_id(new_parent ? new_parent->id() : NodeId());
  const NodeId old_parent_id(old_parent ? old_parent->id() : NodeId());
  DCHECK((node->id().connection_id == id_) ||
         (roots_.count(NodeIdToTransportId(node->id())) > 0) ||
         (new_parent && IsNodeDescendantOfRoots(new_parent)) ||
         (old_parent && IsNodeDescendantOfRoots(old_parent)));
  client()->OnNodeHierarchyChanged(NodeIdToTransportId(node->id()),
                                   NodeIdToTransportId(new_parent_id),
                                   NodeIdToTransportId(old_parent_id),
                                   server_change_id,
                                   NodesToINodes(to_send));
}

void ViewManagerConnection::ProcessNodeViewReplaced(
    const Node* node,
    const View* new_view,
    const View* old_view,
    bool originated_change) {
  if (originated_change || !known_nodes_.count(NodeIdToTransportId(node->id())))
    return;
  const TransportViewId new_view_id = new_view ?
      ViewIdToTransportId(new_view->id()) : 0;
  const TransportViewId old_view_id = old_view ?
      ViewIdToTransportId(old_view->id()) : 0;
  client()->OnNodeViewReplaced(NodeIdToTransportId(node->id()),
                               new_view_id, old_view_id);
}

void ViewManagerConnection::ProcessNodeDeleted(
    const NodeId& node,
    TransportChangeId server_change_id,
    bool originated_change) {
  const bool in_known = known_nodes_.erase(NodeIdToTransportId(node)) > 0;
  const bool in_roots = roots_.erase(NodeIdToTransportId(node)) > 0;

  if (in_roots && roots_.empty())
    roots_.insert(NodeIdToTransportId(InvalidNodeId()));

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

void ViewManagerConnection::OnConnectionError() {
  // TODO(sky): figure out if need to cleanup here if this
  // ViewManagerConnection is the result of a Connect().
}

bool ViewManagerConnection::CanRemoveNodeFromParent(const Node* node) const {
  if (!node)
    return false;

  const Node* parent = node->GetParent();
  if (!parent)
    return false;

  // Always allow the remove if there are no roots. Otherwise the remove is
  // allowed if the parent is a descendant of the roots, or the node and its
  // parent were created by this connection. We explicitly disallow removal of
  // the node from its parent if the parent isn't visible to this connection
  // (not in roots).
  return (roots_.empty() ||
          (IsNodeDescendantOfRoots(parent) ||
           (node->id().connection_id == id_ &&
            parent->id().connection_id == id_)));
}

bool ViewManagerConnection::CanAddNode(const Node* parent,
                                       const Node* child) const {
  if (!parent || !child)
    return false;  // Both nodes must be valid.

  if (child->GetParent() == parent || child->Contains(parent))
    return false;  // Would result in an invalid hierarchy.

  if (roots_.empty())
    return true;  // No restriction if there are no roots.

  if (!IsNodeDescendantOfRoots(parent) && parent->id().connection_id != id_)
    return false;  // |parent| is not visible to this connection.

  // Allow the add if the child is already a descendant of the roots or was
  // created by this connection.
  return (IsNodeDescendantOfRoots(child) || child->id().connection_id == id_);
}

bool ViewManagerConnection::CanDeleteNode(const NodeId& node_id) const {
  return node_id.connection_id == id_;
}

bool ViewManagerConnection::CanDeleteView(const ViewId& view_id) const {
  return view_id.connection_id == id_;
}

bool ViewManagerConnection::CanSetView(const Node* node,
                                       const ViewId& view_id) const {
  if (!node || !IsNodeDescendantOfRoots(node))
    return false;

  const View* view = GetView(view_id);
  return (view && view_id.connection_id == id_) || view_id == ViewId();
}

bool ViewManagerConnection::CanGetNodeTree(const Node* node) const {
  return node &&
      (IsNodeDescendantOfRoots(node) || node->id().connection_id == id_);
}

bool ViewManagerConnection::CanConnect(
    const mojo::Array<uint32_t>& node_ids) const {
  for (size_t i = 0; i < node_ids.size(); ++i) {
    const Node* node = GetNode(NodeIdFromTransportId(node_ids[i]));
    if (!node || node->id().connection_id != id_)
      return false;
  }
  return node_ids.size() > 0;
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

bool ViewManagerConnection::SetViewImpl(Node* node, const ViewId& view_id) {
  DCHECK(node);  // CanSetView() should have verified node exists.
  View* view = GetView(view_id);
  RootNodeManager::ScopedChange change(
      this, root_node_manager_,
      RootNodeManager::CHANGE_TYPE_DONT_ADVANCE_SERVER_CHANGE_ID, false);
  node->SetView(view);
  return true;
}

void ViewManagerConnection::GetUnknownNodesFrom(
    const Node* node,
    std::vector<const Node*>* nodes) {
  const TransportNodeId transport_id = NodeIdToTransportId(node->id());
  if (known_nodes_.count(transport_id) == 1)
    return;
  nodes->push_back(node);
  known_nodes_.insert(transport_id);
  std::vector<const Node*> children(node->GetChildren());
  for (size_t i = 0 ; i < children.size(); ++i)
    GetUnknownNodesFrom(children[i], nodes);
}

void ViewManagerConnection::RemoveFromKnown(const Node* node) {
  if (node->id().connection_id == id_)
    return;
  known_nodes_.erase(NodeIdToTransportId(node->id()));
  std::vector<const Node*> children = node->GetChildren();
  for (size_t i = 0; i < children.size(); ++i)
    RemoveFromKnown(children[i]);
}

bool ViewManagerConnection::IsNodeDescendantOfRoots(const Node* node) const {
  if (roots_.empty())
    return true;
  if (!node)
    return false;
  const TransportNodeId invalid_node_id =
      NodeIdToTransportId(InvalidNodeId());
  for (NodeIdSet::const_iterator i = roots_.begin(); i != roots_.end(); ++i) {
    if (*i == invalid_node_id)
      continue;
    const Node* root = GetNode(NodeIdFromTransportId(*i));
    DCHECK(root);
    if (root->Contains(node))
      return true;
  }
  return false;
}

bool ViewManagerConnection::ShouldNotifyOnHierarchyChange(
    const Node* node,
    const Node** new_parent,
    const Node** old_parent,
    std::vector<const Node*>* to_send) {
  // If the node is not in |roots_| or was never known to this connection then
  // don't notify the client about it.
  if (node->id().connection_id != id_ &&
      known_nodes_.count(NodeIdToTransportId(node->id())) == 0 &&
      !IsNodeDescendantOfRoots(node)) {
    return false;
  }
  if (!IsNodeDescendantOfRoots(*new_parent))
    *new_parent = NULL;
  if (!IsNodeDescendantOfRoots(*old_parent))
    *old_parent = NULL;

  if (*new_parent) {
    // On getting a new parent we may need to communicate new nodes to the
    // client. We do that in the following cases:
    // . New parent is a descendant of the roots. In this case the client
    //   already knows all ancestors, so we only have to communicate descendants
    //   of node the client doesn't know about.
    // . If the client knew about the parent, we have to do the same.
    // . If the client knows about the node and is added to a tree the client
    //   doesn't know about we have to communicate from the root down (the
    //   client is learning about a new root).
    if (root_node_manager_->root()->Contains(*new_parent) ||
        known_nodes_.count(NodeIdToTransportId((*new_parent)->id()))) {
      GetUnknownNodesFrom(node, to_send);
      return true;
    }
    // If parent wasn't known we have to communicate from the root down.
    if (known_nodes_.count(NodeIdToTransportId(node->id()))) {
      // No need to check against |roots_| as client should always know it's
      // |roots_|.
      GetUnknownNodesFrom((*new_parent)->GetRoot(), to_send);
      return true;
    }
  }
  // Otherwise only communicate the change if the node was known. We shouldn't
  // need to communicate any nodes on a remove.
  return known_nodes_.count(NodeIdToTransportId(node->id())) > 0;
}

Array<INode> ViewManagerConnection::NodesToINodes(
    const std::vector<const Node*>& nodes) {
  Array<INode>::Builder array_builder(nodes.size());
  for (size_t i = 0; i < nodes.size(); ++i) {
    INode::Builder node_builder;
    const Node* node = nodes[i];
    DCHECK(known_nodes_.count(NodeIdToTransportId(node->id())) > 0);
    const Node* parent = node->GetParent();
    // If the parent isn't known, it means the parent is not visible to us (not
    // in roots), and should not be sent over.
    if (parent && known_nodes_.count(NodeIdToTransportId(parent->id())) == 0)
      parent = NULL;
    node_builder.set_parent_id(NodeIdToTransportId(
                                   parent ? parent->id() : NodeId()));
    node_builder.set_node_id(NodeIdToTransportId(node->id()));
    node_builder.set_view_id(ViewIdToTransportId(
        node->view() ? node->view()->id() : ViewId()));
    node_builder.set_bounds(node->bounds());
    array_builder[i] = node_builder.Finish();
  }
  return array_builder.Finish();
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
  known_nodes_.insert(transport_node_id);
  callback.Run(true);
}

void ViewManagerConnection::DeleteNode(
    TransportNodeId transport_node_id,
    const Callback<void(bool)>& callback) {
  AllocationScope allocation_scope;
  const NodeId node_id(NodeIdFromTransportId(transport_node_id));
  bool did_delete = CanDeleteNode(node_id);
  if (did_delete) {
    ViewManagerConnection* connection = root_node_manager_->GetConnection(
        node_id.connection_id);
    did_delete = connection && connection->DeleteNodeImpl(this, node_id);
  }
  callback.Run(did_delete);
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
    if (CanAddNode(parent, child)) {
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
    if (CanRemoveNodeFromParent(node)) {
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
  std::vector<const Node*> nodes;
  if (CanGetNodeTree(node)) {
    GetDescendants(node, &nodes);
    for (size_t i = 0; i < nodes.size(); ++i)
      known_nodes_.insert(NodeIdToTransportId(nodes[i]->id()));
  }
  callback.Run(NodesToINodes(nodes));
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
  bool did_delete = CanDeleteView(view_id);
  if (did_delete) {
    ViewManagerConnection* connection = root_node_manager_->GetConnection(
        view_id.connection_id);
    did_delete = (connection && connection->DeleteViewImpl(this, view_id));
  }
  callback.Run(did_delete);
}

void ViewManagerConnection::SetView(
    TransportNodeId transport_node_id,
    TransportViewId transport_view_id,
    const Callback<void(bool)>& callback) {
  Node* node = GetNode(NodeIdFromTransportId(transport_node_id));
  const ViewId view_id(ViewIdFromTransportId(transport_view_id));
  callback.Run(CanSetView(node, view_id) && SetViewImpl(node, view_id));
}

void ViewManagerConnection::SetViewContents(
    TransportViewId view_id,
    ScopedSharedBufferHandle buffer,
    uint32_t buffer_size,
    const Callback<void(bool)>& callback) {
  View* view = GetView(ViewIdFromTransportId(view_id));
  if (!view) {
    callback.Run(false);
    return;
  }
  void* handle_data;
  if (MapBuffer(buffer.get(), 0, buffer_size, &handle_data,
                MOJO_MAP_BUFFER_FLAG_NONE) != MOJO_RESULT_OK) {
    callback.Run(false);
    return;
  }
  SkBitmap bitmap;
  gfx::PNGCodec::Decode(static_cast<const unsigned char*>(handle_data),
                        buffer_size, &bitmap);
  view->SetBitmap(bitmap);
  UnmapBuffer(handle_data);
  callback.Run(true);
}

void ViewManagerConnection::SetNodeBounds(
    TransportNodeId node_id,
    const Rect& bounds,
    const Callback<void(bool)>& callback) {
  if (NodeIdFromTransportId(node_id).connection_id != id_) {
    callback.Run(false);
    return;
  }

  Node* node = GetNode(NodeIdFromTransportId(node_id));
  if (!node) {
    callback.Run(false);
    return;
  }

  RootNodeManager::ScopedChange change(
      this, root_node_manager_,
      RootNodeManager::CHANGE_TYPE_DONT_ADVANCE_SERVER_CHANGE_ID, false);
  gfx::Rect old_bounds = node->window()->bounds();
  node->window()->SetBounds(bounds);
  root_node_manager_->ProcessNodeBoundsChanged(node, old_bounds, bounds);
  callback.Run(true);
}

void ViewManagerConnection::Connect(const String& url,
                                    const Array<uint32_t>& node_ids,
                                    const Callback<void(bool)>& callback) {
  const bool success = CanConnect(node_ids);
  if (success)
    root_node_manager_->Connect(url, node_ids);
  callback.Run(success);
}

void ViewManagerConnection::OnNodeHierarchyChanged(const Node* node,
                                                   const Node* new_parent,
                                                   const Node* old_parent) {
  root_node_manager_->ProcessNodeHierarchyChanged(node, new_parent, old_parent);
}

void ViewManagerConnection::OnNodeViewReplaced(const Node* node,
                                               const View* new_view,
                                               const View* old_view) {
  root_node_manager_->ProcessNodeViewReplaced(node, new_view, old_view);
}

void ViewManagerConnection::OnConnectionEstablished() {
  DCHECK_EQ(0, id_);  // Should only get OnConnectionEstablished() once.

  id_ = root_node_manager_->GetAndAdvanceNextConnectionId();

  root_node_manager_->AddConnection(this);

  std::vector<const Node*> to_send;
  if (roots_.empty()) {
    GetUnknownNodesFrom(root_node_manager_->root(), &to_send);
  } else {
    for (NodeIdSet::const_iterator i = roots_.begin(); i != roots_.end(); ++i)
      GetUnknownNodesFrom(GetNode(NodeIdFromTransportId(*i)), &to_send);
  }

  AllocationScope allocation_scope;
  client()->OnViewManagerConnectionEstablished(
      id_,
      root_node_manager_->next_server_change_id(),
      NodesToINodes(to_send));
}

}  // namespace service
}  // namespace view_manager
}  // namespace mojo
