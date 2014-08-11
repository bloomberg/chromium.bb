// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/default_access_policy.h"

#include "mojo/services/view_manager/access_policy_delegate.h"
#include "mojo/services/view_manager/node.h"

namespace mojo {
namespace service {

DefaultAccessPolicy::DefaultAccessPolicy(ConnectionSpecificId connection_id,
                                         AccessPolicyDelegate* delegate)
    : connection_id_(connection_id),
      delegate_(delegate) {
}

DefaultAccessPolicy::~DefaultAccessPolicy() {
}

bool DefaultAccessPolicy::CanRemoveNodeFromParent(const Node* node) const {
  if (!WasCreatedByThisConnection(node))
    return false;  // Can only unparent nodes we created.

  const Node* parent = node->GetParent();
  return IsNodeInRoots(parent) || WasCreatedByThisConnection(parent);
}

bool DefaultAccessPolicy::CanAddNode(const Node* parent,
                                     const Node* child) const {
  return WasCreatedByThisConnection(child) &&
      (IsNodeInRoots(parent) ||
       (WasCreatedByThisConnection(parent) &&
        !delegate_->IsNodeRootOfAnotherConnectionForAccessPolicy(parent)));
}

bool DefaultAccessPolicy::CanReorderNode(const Node* node,
                                         const Node* relative_node,
                                         OrderDirection direction) const {
  return WasCreatedByThisConnection(node) &&
      WasCreatedByThisConnection(relative_node);
}

bool DefaultAccessPolicy::CanDeleteNode(const Node* node) const {
  return WasCreatedByThisConnection(node);
}

bool DefaultAccessPolicy::CanSetFocus(const Node* node) const {
  // TODO(beng): security.
  return true;
}

bool DefaultAccessPolicy::CanGetNodeTree(const Node* node) const {
  return WasCreatedByThisConnection(node) || IsNodeInRoots(node);
}

bool DefaultAccessPolicy::CanDescendIntoNodeForNodeTree(
    const Node* node) const {
  return WasCreatedByThisConnection(node) &&
      !delegate_->IsNodeRootOfAnotherConnectionForAccessPolicy(node);
}

bool DefaultAccessPolicy::CanEmbed(const Node* node) const {
  return WasCreatedByThisConnection(node);
}

bool DefaultAccessPolicy::CanChangeNodeVisibility(const Node* node) const {
  return WasCreatedByThisConnection(node) || IsNodeInRoots(node);
}

bool DefaultAccessPolicy::CanSetNodeContents(const Node* node) const {
  // Once a node embeds another app, the embedder app is no longer able to
  // call SetNodeContents() - this ability is transferred to the embedded app.
  if (delegate_->IsNodeRootOfAnotherConnectionForAccessPolicy(node))
    return false;
  return WasCreatedByThisConnection(node) || IsNodeInRoots(node);
}

bool DefaultAccessPolicy::CanSetNodeBounds(const Node* node) const {
  return WasCreatedByThisConnection(node);
}

bool DefaultAccessPolicy::ShouldNotifyOnHierarchyChange(
    const Node* node,
    const Node** new_parent,
    const Node** old_parent) const {
  if (!WasCreatedByThisConnection(node))
    return false;

  if (*new_parent && !WasCreatedByThisConnection(*new_parent) &&
      !IsNodeInRoots(*new_parent)) {
    *new_parent = NULL;
  }

  if (*old_parent && !WasCreatedByThisConnection(*old_parent) &&
      !IsNodeInRoots(*old_parent)) {
    *old_parent = NULL;
  }
  return true;
}

bool DefaultAccessPolicy::ShouldSendViewDeleted(const ViewId& view_id) const {
  return view_id.connection_id == connection_id_;
}

bool DefaultAccessPolicy::IsNodeInRoots(const Node* node) const {
  return delegate_->GetRootsForAccessPolicy().count(
      NodeIdToTransportId(node->id())) > 0;
}

}  // namespace service
}  // namespace mojo
