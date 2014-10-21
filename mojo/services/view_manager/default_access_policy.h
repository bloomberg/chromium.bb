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
  ~DefaultAccessPolicy() override;

  // AccessPolicy:
  bool CanRemoveViewFromParent(const ServerView* view) const override;
  bool CanAddView(const ServerView* parent,
                  const ServerView* child) const override;
  bool CanReorderView(const ServerView* view,
                      const ServerView* relative_view,
                      OrderDirection direction) const override;
  bool CanDeleteView(const ServerView* view) const override;
  bool CanGetViewTree(const ServerView* view) const override;
  bool CanDescendIntoViewForViewTree(const ServerView* view) const override;
  bool CanEmbed(const ServerView* view) const override;
  bool CanChangeViewVisibility(const ServerView* view) const override;
  bool CanSetViewSurfaceId(const ServerView* view) const override;
  bool CanSetViewBounds(const ServerView* view) const override;
  bool ShouldNotifyOnHierarchyChange(
      const ServerView* view,
      const ServerView** new_parent,
      const ServerView** old_parent) const override;

 private:
  bool IsViewInRoots(const ServerView* view) const;

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
