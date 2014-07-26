// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/window_manager_access_policy.h"

#include "mojo/services/view_manager/access_policy_delegate.h"
#include "mojo/services/view_manager/node.h"
#include "mojo/services/view_manager/view.h"

namespace mojo {
namespace service {

// TODO(sky): document why this differs from default for each case. Maybe want
// to subclass DefaultAccessPolicy.

WindowManagerAccessPolicy::WindowManagerAccessPolicy(
    ConnectionSpecificId connection_id,
    AccessPolicyDelegate* delegate)
    : connection_id_(connection_id),
      delegate_(delegate) {
}

WindowManagerAccessPolicy::~WindowManagerAccessPolicy() {
}

bool WindowManagerAccessPolicy::CanRemoveNodeFromParent(
    const Node* node) const {
  return true;
}

bool WindowManagerAccessPolicy::CanAddNode(const Node* parent,
                                           const Node* child) const {
  return true;
}

bool WindowManagerAccessPolicy::CanReorderNode(const Node* node,
                                               const Node* relative_node,
                                               OrderDirection direction) const {
  return true;
}

bool WindowManagerAccessPolicy::CanDeleteNode(const Node* node) const {
  return node->id().connection_id == connection_id_;
}

bool WindowManagerAccessPolicy::CanDeleteView(const View* view) const {
  return view->id().connection_id == connection_id_;
}

bool WindowManagerAccessPolicy::CanSetView(const Node* node,
                                           const View* view) const {
  return !view || view->id().connection_id == connection_id_;
}

bool WindowManagerAccessPolicy::CanSetFocus(const Node* node) const {
  // TODO(beng): security.
  return true;
}

bool WindowManagerAccessPolicy::CanGetNodeTree(const Node* node) const {
  return true;
}

bool WindowManagerAccessPolicy::CanDescendIntoNodeForNodeTree(
    const Node* node) const {
  return true;
}

bool WindowManagerAccessPolicy::CanEmbed(const Node* node) const {
  return node->id().connection_id == connection_id_;
}

bool WindowManagerAccessPolicy::CanChangeNodeVisibility(
    const Node* node) const {
  return node->id().connection_id == connection_id_;
}

bool WindowManagerAccessPolicy::CanSetViewContents(const View* view) const {
  return view->id().connection_id == connection_id_;
}

bool WindowManagerAccessPolicy::CanSetNodeBounds(const Node* node) const {
  return node->id().connection_id == connection_id_;
}

bool WindowManagerAccessPolicy::ShouldNotifyOnHierarchyChange(
    const Node* node,
    const Node** new_parent,
    const Node** old_parent) const {
  // Notify if we've already told the window manager about the node, or if we've
  // already told the window manager about the parent. The later handles the
  // case of a node that wasn't parented to the root getting added to the root.
  return IsNodeKnown(node) || (*new_parent && IsNodeKnown(*new_parent));
}

bool WindowManagerAccessPolicy::ShouldSendViewDeleted(
    const ViewId& view_id) const {
  return true;
}

bool WindowManagerAccessPolicy::IsNodeKnown(const Node* node) const {
  return delegate_->IsNodeKnownForAccessPolicy(node);
}

Id WindowManagerAccessPolicy::GetViewIdToSend(const Node* node,
                                              const View* view) const {
  return ViewIdToTransportId(view->id());
}

}  // namespace service
}  // namespace mojo
