// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view_manager_service_impl.h"

#include "base/bind.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/input_events/input_events_type_converters.h"
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

ViewManagerServiceImpl::ViewManagerServiceImpl(
    RootNodeManager* root_node_manager,
    ConnectionSpecificId creator_id,
    const std::string& creator_url,
    const std::string& url,
    const NodeId& root_id)
    : root_node_manager_(root_node_manager),
      id_(root_node_manager_->GetAndAdvanceNextConnectionId()),
      url_(url),
      creator_id_(creator_id),
      creator_url_(creator_url),
      delete_on_connection_error_(false) {
  if (root_id != InvalidNodeId()) {
    CHECK(GetNode(root_id));
    roots_.insert(NodeIdToTransportId(root_id));
  }
}

ViewManagerServiceImpl::~ViewManagerServiceImpl() {
  // Delete any views we created.
  while (!view_map_.empty()) {
    bool result = DeleteViewImpl(this, view_map_.begin()->second->id());
    DCHECK(result);
  }

  // Ditto the nodes.
  if (!node_map_.empty()) {
    RootNodeManager::ScopedChange change(this, root_node_manager_, true);
    while (!node_map_.empty())
      delete node_map_.begin()->second;
  }

  root_node_manager_->RemoveConnection(this);
}

const Node* ViewManagerServiceImpl::GetNode(const NodeId& id) const {
  if (id_ == id.connection_id) {
    NodeMap::const_iterator i = node_map_.find(id.node_id);
    return i == node_map_.end() ? NULL : i->second;
  }
  return root_node_manager_->GetNode(id);
}

const View* ViewManagerServiceImpl::GetView(const ViewId& id) const {
  if (id_ == id.connection_id) {
    ViewMap::const_iterator i = view_map_.find(id.view_id);
    return i == view_map_.end() ? NULL : i->second;
  }
  return root_node_manager_->GetView(id);
}

bool ViewManagerServiceImpl::HasRoot(const NodeId& id) const {
  return roots_.find(NodeIdToTransportId(id)) != roots_.end();
}

void ViewManagerServiceImpl::OnViewManagerServiceImplDestroyed(
    ConnectionSpecificId id) {
  if (creator_id_ == id)
    creator_id_ = kRootConnection;
}

void ViewManagerServiceImpl::ProcessNodeBoundsChanged(
    const Node* node,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    bool originated_change) {
  if (originated_change)
    return;
  Id node_id = NodeIdToTransportId(node->id());
  if (known_nodes_.count(node_id) > 0) {
    client()->OnNodeBoundsChanged(node_id,
                                  Rect::From(old_bounds),
                                  Rect::From(new_bounds));
  }
}

void ViewManagerServiceImpl::ProcessNodeHierarchyChanged(
    const Node* node,
    const Node* new_parent,
    const Node* old_parent,
    bool originated_change) {
  if (known_nodes_.count(NodeIdToTransportId(node->id())) > 0) {
    if (originated_change)
      return;
    if (node->id().connection_id != id_ && !IsNodeDescendantOfRoots(node)) {
      // Node was a descendant of roots and is no longer, treat it as though the
      // node was deleted.
      RemoveFromKnown(node, NULL);
      client()->OnNodeDeleted(NodeIdToTransportId(node->id()));
      root_node_manager_->OnConnectionMessagedClient(id_);
      return;
    }
  }

  if (originated_change || root_node_manager_->is_processing_delete_node())
    return;
  std::vector<const Node*> to_send;
  if (!ShouldNotifyOnHierarchyChange(node, &new_parent, &old_parent,
                                     &to_send)) {
    return;
  }
  const NodeId new_parent_id(new_parent ? new_parent->id() : NodeId());
  const NodeId old_parent_id(old_parent ? old_parent->id() : NodeId());
  DCHECK((node->id().connection_id == id_) ||
         (roots_.count(NodeIdToTransportId(node->id())) > 0) ||
         (new_parent && IsNodeDescendantOfRoots(new_parent)) ||
         (old_parent && IsNodeDescendantOfRoots(old_parent)));
  client()->OnNodeHierarchyChanged(NodeIdToTransportId(node->id()),
                                   NodeIdToTransportId(new_parent_id),
                                   NodeIdToTransportId(old_parent_id),
                                   NodesToNodeDatas(to_send));
}

void ViewManagerServiceImpl::ProcessNodeReorder(const Node* node,
                                                const Node* relative_node,
                                                OrderDirection direction,
                                                bool originated_change) {
  if (originated_change ||
      !known_nodes_.count(NodeIdToTransportId(node->id())) ||
      !known_nodes_.count(NodeIdToTransportId(relative_node->id()))) {
    return;
  }

  client()->OnNodeReordered(NodeIdToTransportId(node->id()),
                            NodeIdToTransportId(relative_node->id()),
                            direction);
}

void ViewManagerServiceImpl::ProcessNodeViewReplaced(
    const Node* node,
    const View* new_view,
    const View* old_view,
    bool originated_change) {
  if (originated_change || !known_nodes_.count(NodeIdToTransportId(node->id())))
    return;
  const Id new_view_id = new_view ?
      ViewIdToTransportId(new_view->id()) : 0;
  const Id old_view_id = old_view ?
      ViewIdToTransportId(old_view->id()) : 0;
  client()->OnNodeViewReplaced(NodeIdToTransportId(node->id()),
                               new_view_id, old_view_id);
}

void ViewManagerServiceImpl::ProcessNodeDeleted(const NodeId& node,
                                                bool originated_change) {
  node_map_.erase(node.node_id);

  const bool in_known = known_nodes_.erase(NodeIdToTransportId(node)) > 0;
  const bool in_roots = roots_.erase(NodeIdToTransportId(node)) > 0;

  if (in_roots && roots_.empty())
    roots_.insert(NodeIdToTransportId(InvalidNodeId()));

  if (originated_change)
    return;

  if (in_known) {
    client()->OnNodeDeleted(NodeIdToTransportId(node));
    root_node_manager_->OnConnectionMessagedClient(id_);
  } else if (root_node_manager_->IsProcessingChange() &&
             !root_node_manager_->DidConnectionMessageClient(id_)) {
    root_node_manager_->OnConnectionMessagedClient(id_);
  }
}

void ViewManagerServiceImpl::ProcessViewDeleted(const ViewId& view,
                                                bool originated_change) {
  if (originated_change)
    return;
  client()->OnViewDeleted(ViewIdToTransportId(view));
}

void ViewManagerServiceImpl::ProcessFocusChanged(const Node* focused_node,
                                                 const Node* blurred_node,
                                                 bool originated_change) {
  if (originated_change)
    return;

  Id focused_id = 0;
  Id blurred_id = 0;
  if (focused_node) {
    Id focused_node_id = NodeIdToTransportId(focused_node->id());
    if (known_nodes_.count(focused_node_id) > 0)
      focused_id = focused_node_id;
  }
  if (blurred_node) {
    Id blurred_node_id = NodeIdToTransportId(blurred_node->id());
    if (known_nodes_.count(blurred_node_id) > 0)
      blurred_id = blurred_node_id;
  }

  if (focused_id != 0 || blurred_id != 0)
    client()->OnFocusChanged(focused_id, blurred_id);
}

void ViewManagerServiceImpl::OnConnectionError() {
  if (delete_on_connection_error_)
    delete this;
}

bool ViewManagerServiceImpl::CanRemoveNodeFromParent(const Node* node) const {
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

bool ViewManagerServiceImpl::CanAddNode(const Node* parent,
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

bool ViewManagerServiceImpl::CanReorderNode(const Node* node,
                                            const Node* relative_node,
                                            OrderDirection direction) const {
  if (!node || !relative_node)
    return false;

  if (node->id().connection_id != id_)
    return false;

  const Node* parent = node->GetParent();
  if (!parent || parent != relative_node->GetParent())
    return false;

  if (known_nodes_.count(NodeIdToTransportId(parent->id())) == 0)
    return false;

  std::vector<const Node*> children = parent->GetChildren();
  const size_t child_i =
      std::find(children.begin(), children.end(), node) - children.begin();
  const size_t target_i =
      std::find(children.begin(), children.end(), relative_node) -
      children.begin();
  if ((direction == ORDER_DIRECTION_ABOVE && child_i == target_i + 1) ||
      (direction == ORDER_DIRECTION_BELOW && child_i + 1 == target_i)) {
    return false;
  }

  return true;
}

bool ViewManagerServiceImpl::CanDeleteNode(const NodeId& node_id) const {
  return node_id.connection_id == id_;
}

bool ViewManagerServiceImpl::CanDeleteView(const ViewId& view_id) const {
  return view_id.connection_id == id_;
}

bool ViewManagerServiceImpl::CanSetView(const Node* node,
                                        const ViewId& view_id) const {
  if (!node || !IsNodeDescendantOfRoots(node))
    return false;

  const View* view = GetView(view_id);
  return (view && view_id.connection_id == id_) || view_id == ViewId();
}

bool ViewManagerServiceImpl::CanSetFocus(const Node* node) const {
  // TODO(beng): security.
  return true;
}

bool ViewManagerServiceImpl::CanGetNodeTree(const Node* node) const {
  return node &&
      (IsNodeDescendantOfRoots(node) || node->id().connection_id == id_);
}

bool ViewManagerServiceImpl::CanEmbed(Id transport_node_id) const {
  const Node* node = GetNode(NodeIdFromTransportId(transport_node_id));
  return node && node->id().connection_id == id_;
}

bool ViewManagerServiceImpl::CanSetNodeVisibility(const Node* node,
                                                  bool visibile) const {
  return node &&
      (node->id().connection_id == id_ ||
       roots_.find(NodeIdToTransportId(node->id())) != roots_.end()) &&
      node->IsVisible() != visibile;
}

bool ViewManagerServiceImpl::DeleteNodeImpl(ViewManagerServiceImpl* source,
                                            const NodeId& node_id) {
  DCHECK_EQ(node_id.connection_id, id_);
  Node* node = GetNode(node_id);
  if (!node)
    return false;
  RootNodeManager::ScopedChange change(source, root_node_manager_, true);
  delete node;
  return true;
}

bool ViewManagerServiceImpl::DeleteViewImpl(ViewManagerServiceImpl* source,
                                            const ViewId& view_id) {
  DCHECK_EQ(view_id.connection_id, id_);
  View* view = GetView(view_id);
  if (!view)
    return false;
  RootNodeManager::ScopedChange change(source, root_node_manager_, false);
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

bool ViewManagerServiceImpl::SetViewImpl(Node* node, const ViewId& view_id) {
  DCHECK(node);  // CanSetView() should have verified node exists.
  View* view = GetView(view_id);
  RootNodeManager::ScopedChange change(this, root_node_manager_, false);
  node->SetView(view);
  return true;
}

void ViewManagerServiceImpl::GetUnknownNodesFrom(
    const Node* node,
    std::vector<const Node*>* nodes) {
  const Id transport_id = NodeIdToTransportId(node->id());
  if (known_nodes_.count(transport_id) == 1)
    return;
  nodes->push_back(node);
  known_nodes_.insert(transport_id);
  std::vector<const Node*> children(node->GetChildren());
  for (size_t i = 0 ; i < children.size(); ++i)
    GetUnknownNodesFrom(children[i], nodes);
}

void ViewManagerServiceImpl::RemoveFromKnown(const Node* node,
                                             std::vector<Node*>* local_nodes) {
  if (node->id().connection_id == id_) {
    if (local_nodes)
      local_nodes->push_back(GetNode(node->id()));
    return;
  }
  known_nodes_.erase(NodeIdToTransportId(node->id()));
  std::vector<const Node*> children = node->GetChildren();
  for (size_t i = 0; i < children.size(); ++i)
    RemoveFromKnown(children[i], local_nodes);
}

void ViewManagerServiceImpl::AddRoot(const NodeId& node_id) {
  const Id transport_node_id(NodeIdToTransportId(node_id));
  CHECK(roots_.count(transport_node_id) == 0);

  std::vector<const Node*> to_send;
  CHECK_EQ(creator_id_, node_id.connection_id);
  roots_.insert(transport_node_id);
  Node* node = GetNode(node_id);
  CHECK(node);
  if (known_nodes_.count(transport_node_id) == 0) {
    GetUnknownNodesFrom(node, &to_send);
  } else {
    // Even though the connection knows about the new root we need to tell it
    // |node| is now a root.
    to_send.push_back(node);
  }

  client()->OnRootAdded(NodesToNodeDatas(to_send));
  root_node_manager_->OnConnectionMessagedClient(id_);
}

void ViewManagerServiceImpl::RemoveRoot(const NodeId& node_id) {
  const Id transport_node_id(NodeIdToTransportId(node_id));
  CHECK(roots_.count(transport_node_id) > 0);

  roots_.erase(transport_node_id);
  if (roots_.empty())
    roots_.insert(NodeIdToTransportId(InvalidNodeId()));

  // No need to do anything if we created the node.
  if (node_id.connection_id == id_)
    return;

  client()->OnNodeDeleted(transport_node_id);
  root_node_manager_->OnConnectionMessagedClient(id_);

  // This connection no longer knows about the node. Unparent any nodes that
  // were parented to nodes in the root.
  std::vector<Node*> local_nodes;
  RemoveFromKnown(GetNode(node_id), &local_nodes);
  for (size_t i = 0; i < local_nodes.size(); ++i)
    local_nodes[i]->GetParent()->Remove(local_nodes[i]);
}

bool ViewManagerServiceImpl::IsNodeDescendantOfRoots(const Node* node) const {
  if (roots_.empty())
    return true;
  if (!node)
    return false;
  const Id invalid_node_id =
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

bool ViewManagerServiceImpl::ShouldNotifyOnHierarchyChange(
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

Array<NodeDataPtr> ViewManagerServiceImpl::NodesToNodeDatas(
    const std::vector<const Node*>& nodes) {
  Array<NodeDataPtr> array(nodes.size());
  for (size_t i = 0; i < nodes.size(); ++i) {
    const Node* node = nodes[i];
    DCHECK(known_nodes_.count(NodeIdToTransportId(node->id())) > 0);
    const Node* parent = node->GetParent();
    // If the parent isn't known, it means the parent is not visible to us (not
    // in roots), and should not be sent over.
    if (parent && known_nodes_.count(NodeIdToTransportId(parent->id())) == 0)
      parent = NULL;
    NodeDataPtr inode(NodeData::New());
    inode->parent_id = NodeIdToTransportId(parent ? parent->id() : NodeId());
    inode->node_id = NodeIdToTransportId(node->id());
    inode->view_id =
        ViewIdToTransportId(node->view() ? node->view()->id() : ViewId());
    inode->bounds = Rect::From(node->bounds());
    array[i] = inode.Pass();
  }
  return array.Pass();
}

void ViewManagerServiceImpl::CreateNode(
    Id transport_node_id,
    const Callback<void(ErrorCode)>& callback) {
  const NodeId node_id(NodeIdFromTransportId(transport_node_id));
  ErrorCode error_code = ERROR_CODE_NONE;
  if (node_id.connection_id != id_) {
    error_code = ERROR_CODE_ILLEGAL_ARGUMENT;
  } else if (node_map_.find(node_id.node_id) != node_map_.end()) {
    error_code = ERROR_CODE_VALUE_IN_USE;
  } else {
    node_map_[node_id.node_id] = new Node(root_node_manager_, node_id);
    known_nodes_.insert(transport_node_id);
  }
  callback.Run(error_code);
}

void ViewManagerServiceImpl::DeleteNode(
    Id transport_node_id,
    const Callback<void(bool)>& callback) {
  const NodeId node_id(NodeIdFromTransportId(transport_node_id));
  bool success = false;
  if (CanDeleteNode(node_id)) {
    ViewManagerServiceImpl* connection = root_node_manager_->GetConnection(
        node_id.connection_id);
    success = connection && connection->DeleteNodeImpl(this, node_id);
  }
  callback.Run(success);
}

void ViewManagerServiceImpl::AddNode(
    Id parent_id,
    Id child_id,
    const Callback<void(bool)>& callback) {
  bool success = false;
  Node* parent = GetNode(NodeIdFromTransportId(parent_id));
  Node* child = GetNode(NodeIdFromTransportId(child_id));
  if (CanAddNode(parent, child)) {
    success = true;
    RootNodeManager::ScopedChange change(this, root_node_manager_, false);
    parent->Add(child);
  }
  callback.Run(success);
}

void ViewManagerServiceImpl::RemoveNodeFromParent(
    Id node_id,
    const Callback<void(bool)>& callback) {
  bool success = false;
  Node* node = GetNode(NodeIdFromTransportId(node_id));
  if (CanRemoveNodeFromParent(node)) {
    success = true;
    RootNodeManager::ScopedChange change(this, root_node_manager_, false);
    node->GetParent()->Remove(node);
  }
  callback.Run(success);
}

void ViewManagerServiceImpl::ReorderNode(Id node_id,
                                         Id relative_node_id,
                                         OrderDirection direction,
                                         const Callback<void(bool)>& callback) {
  bool success = false;
  Node* node = GetNode(NodeIdFromTransportId(node_id));
  Node* relative_node = GetNode(NodeIdFromTransportId(relative_node_id));
  if (CanReorderNode(node, relative_node, direction)) {
    success = true;
    RootNodeManager::ScopedChange change(this, root_node_manager_, false);
    node->GetParent()->Reorder(node, relative_node, direction);
    root_node_manager_->ProcessNodeReorder(node, relative_node, direction);
  }
  callback.Run(success);
}

void ViewManagerServiceImpl::GetNodeTree(
    Id node_id,
    const Callback<void(Array<NodeDataPtr>)>& callback) {
  Node* node = GetNode(NodeIdFromTransportId(node_id));
  std::vector<const Node*> nodes;
  if (CanGetNodeTree(node)) {
    GetDescendants(node, &nodes);
    for (size_t i = 0; i < nodes.size(); ++i)
      known_nodes_.insert(NodeIdToTransportId(nodes[i]->id()));
  }
  callback.Run(NodesToNodeDatas(nodes));
}

void ViewManagerServiceImpl::CreateView(
    Id transport_view_id,
    const Callback<void(bool)>& callback) {
  const ViewId view_id(ViewIdFromTransportId(transport_view_id));
  if (view_id.connection_id != id_ || view_map_.count(view_id.view_id)) {
    callback.Run(false);
    return;
  }
  view_map_[view_id.view_id] = new View(view_id);
  callback.Run(true);
}

void ViewManagerServiceImpl::DeleteView(
    Id transport_view_id,
    const Callback<void(bool)>& callback) {
  const ViewId view_id(ViewIdFromTransportId(transport_view_id));
  bool did_delete = CanDeleteView(view_id);
  if (did_delete) {
    ViewManagerServiceImpl* connection = root_node_manager_->GetConnection(
        view_id.connection_id);
    did_delete = (connection && connection->DeleteViewImpl(this, view_id));
  }
  callback.Run(did_delete);
}

void ViewManagerServiceImpl::SetView(Id transport_node_id,
                                     Id transport_view_id,
                                     const Callback<void(bool)>& callback) {
  Node* node = GetNode(NodeIdFromTransportId(transport_node_id));
  const ViewId view_id(ViewIdFromTransportId(transport_view_id));
  callback.Run(CanSetView(node, view_id) && SetViewImpl(node, view_id));
}

void ViewManagerServiceImpl::SetViewContents(
    Id view_id,
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

void ViewManagerServiceImpl::SetFocus(Id node_id,
                                      const Callback<void(bool)> & callback) {
  bool success = false;
  Node* node = GetNode(NodeIdFromTransportId(node_id));
  if (CanSetFocus(node)) {
    success = true;
    node->window()->Focus();
  }
  callback.Run(success);
}

void ViewManagerServiceImpl::SetNodeBounds(
    Id node_id,
    RectPtr bounds,
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

  RootNodeManager::ScopedChange change(this, root_node_manager_, false);
  gfx::Rect old_bounds = node->window()->bounds();
  node->window()->SetBounds(bounds.To<gfx::Rect>());
  callback.Run(true);
}

void ViewManagerServiceImpl::SetNodeVisibility(
    Id transport_node_id,
    bool visible,
    const Callback<void(bool)>& callback) {
  const NodeId node_id(NodeIdFromTransportId(transport_node_id));
  Node* node = GetNode(node_id);
  const bool success = CanSetNodeVisibility(node, visible);
  if (success) {
    DCHECK(node);
    node->SetVisible(visible);
  }
  // TODO(sky): need to notify of visibility changes.
  callback.Run(success);
}

void ViewManagerServiceImpl::Embed(const String& url,
                                   Id transport_node_id,
                                   const Callback<void(bool)>& callback) {
  bool success = CanEmbed(transport_node_id);
  if (success) {
    // Only allow a node to be the root for one connection.
    const NodeId node_id(NodeIdFromTransportId(transport_node_id));
    ViewManagerServiceImpl* connection_by_url =
        root_node_manager_->GetConnectionByCreator(id_, url.To<std::string>());
    ViewManagerServiceImpl* connection_with_node_as_root =
        root_node_manager_->GetConnectionWithRoot(node_id);
    if ((connection_by_url != connection_with_node_as_root ||
         (!connection_by_url && !connection_with_node_as_root)) &&
        (!connection_by_url || !connection_by_url->HasRoot(node_id))) {
      RootNodeManager::ScopedChange change(this, root_node_manager_, true);
      // Never message the originating connection.
      root_node_manager_->OnConnectionMessagedClient(id_);
      if (connection_with_node_as_root)
        connection_with_node_as_root->RemoveRoot(node_id);
      if (connection_by_url)
        connection_by_url->AddRoot(node_id);
      else
        root_node_manager_->Embed(id_, url, transport_node_id);
    } else {
      success = false;
    }
  }
  callback.Run(success);
}

void ViewManagerServiceImpl::DispatchOnViewInputEvent(Id transport_view_id,
                                                      EventPtr event) {
  // We only allow the WM to dispatch events. At some point this function will
  // move to a separate interface and the check can go away.
  if (id_ != kWindowManagerConnection)
    return;

  const ViewId view_id(ViewIdFromTransportId(transport_view_id));
  ViewManagerServiceImpl* connection = root_node_manager_->GetConnection(
      view_id.connection_id);
  if (connection) {
    connection->client()->OnViewInputEvent(
        transport_view_id,
        event.Pass(),
        base::Bind(&base::DoNothing));
  }
}

void ViewManagerServiceImpl::OnConnectionEstablished() {
  root_node_manager_->AddConnection(this);

  std::vector<const Node*> to_send;
  if (roots_.empty()) {
    GetUnknownNodesFrom(root_node_manager_->root(), &to_send);
  } else {
    for (NodeIdSet::const_iterator i = roots_.begin(); i != roots_.end(); ++i)
      GetUnknownNodesFrom(GetNode(NodeIdFromTransportId(*i)), &to_send);
  }

  client()->OnViewManagerConnectionEstablished(
      id_,
      creator_url_,
      NodesToNodeDatas(to_send));
}

}  // namespace service
}  // namespace view_manager
}  // namespace mojo
