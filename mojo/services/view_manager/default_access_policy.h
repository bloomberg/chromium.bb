// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_DEFAULT_ACCESS_POLICY_H_
#define MOJO_SERVICES_VIEW_MANAGER_DEFAULT_ACCESS_POLICY_H_

#include "base/basictypes.h"
#include "mojo/services/view_manager/access_policy.h"

namespace mojo {
namespace service {

class AccessPolicyDelegate;

// AccessPolicy for all connections, except the window manager.
class DefaultAccessPolicy : public AccessPolicy {
 public:
  DefaultAccessPolicy(ConnectionSpecificId connection_id,
                      AccessPolicyDelegate* delegate);
  virtual ~DefaultAccessPolicy();

  // AccessPolicy:
  virtual bool CanRemoveNodeFromParent(const Node* node) const OVERRIDE;
  virtual bool CanAddNode(const Node* parent, const Node* child) const OVERRIDE;
  virtual bool CanReorderNode(const Node* node,
                              const Node* relative_node,
                              OrderDirection direction) const OVERRIDE;
  virtual bool CanDeleteNode(const Node* node) const OVERRIDE;
  virtual bool CanGetNodeTree(const Node* node) const OVERRIDE;
  virtual bool CanDescendIntoNodeForNodeTree(const Node* node) const OVERRIDE;
  virtual bool CanEmbed(const Node* node) const OVERRIDE;
  virtual bool CanChangeNodeVisibility(const Node* node) const OVERRIDE;
  virtual bool CanSetNodeContents(const Node* node) const OVERRIDE;
  virtual bool CanSetNodeBounds(const Node* node) const OVERRIDE;
  virtual bool ShouldNotifyOnHierarchyChange(
      const Node* node,
      const Node** new_parent,
      const Node** old_parent) const OVERRIDE;
  virtual bool ShouldSendViewDeleted(const ViewId& view_id) const OVERRIDE;

 private:
  bool IsNodeInRoots(const Node* node) const;

  template <typename T>
  bool WasCreatedByThisConnection(const T* t) const {
    return t->id().connection_id == connection_id_;
  }

  const ConnectionSpecificId connection_id_;
  AccessPolicyDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(DefaultAccessPolicy);
};

}  // namespace service
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_DEFAULT_ACCESS_POLICY_H_
