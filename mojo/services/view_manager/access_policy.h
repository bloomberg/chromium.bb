// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_ACCESS_POLICY_H_
#define MOJO_SERVICES_VIEW_MANAGER_ACCESS_POLICY_H_

#include "mojo/services/public/interfaces/view_manager/view_manager_constants.mojom.h"
#include "mojo/services/view_manager/ids.h"

namespace mojo {
namespace service {

class Node;
class View;

// AccessPolicy is used by ViewManagerServiceImpl to determine what a connection
// is allowed to do.
class AccessPolicy {
 public:
  virtual ~AccessPolicy() {}

  // Unless otherwise mentioned all arguments have been validated. That is the
  // |node| and/or |view| arguments are non-null unless otherwise stated (eg
  // CanSetView() is allowed to take a NULL view).
  virtual bool CanRemoveNodeFromParent(const Node* node) const = 0;
  virtual bool CanAddNode(const Node* parent, const Node* child) const = 0;
  virtual bool CanReorderNode(const Node* node,
                              const Node* relative_node,
                              OrderDirection direction) const = 0;
  virtual bool CanDeleteNode(const Node* node) const = 0;
  virtual bool CanDeleteView(const View* view) const = 0;
  // Note |view| may be NULL here.
  virtual bool CanSetView(const Node* node, const View* view) const = 0;
  virtual bool CanSetFocus(const Node* node) const = 0;
  virtual bool CanGetNodeTree(const Node* node) const = 0;
  // Used when building a node tree (GetNodeTree()) to decide if we should
  // descend into |node|.
  virtual bool CanDescendIntoNodeForNodeTree(const Node* node) const = 0;
  virtual bool CanEmbed(const Node* node) const = 0;
  virtual bool CanChangeNodeVisibility(const Node* node) const = 0;
  virtual bool CanSetViewContents(const View* view) const = 0;
  virtual bool CanSetNodeBounds(const Node* node) const = 0;

  // Returns whether the connection should notify on a hierarchy change.
  // |new_parent| and |old_parent| are initially set to the new and old parents
  // but may be altered so that the client only sees a certain set of nodes.
  virtual bool ShouldNotifyOnHierarchyChange(
      const Node* node,
      const Node** new_parent,
      const Node** old_parent) const = 0;

  // Returns the Id of the view to send to the client. |view| is or was
  // associated with |node|.
  virtual Id GetViewIdToSend(const Node* node, const View* view) const = 0;

  virtual bool ShouldSendViewDeleted(const ViewId& view_id) const = 0;
};

}  // namespace service
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_ACCESS_POLICY_H_
