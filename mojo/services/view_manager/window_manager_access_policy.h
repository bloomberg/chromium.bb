// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_WINDOW_MANAGER_ACCESS_POLICY_H_
#define MOJO_SERVICES_VIEW_MANAGER_WINDOW_MANAGER_ACCESS_POLICY_H_

#include "base/basictypes.h"
#include "mojo/services/view_manager/access_policy.h"

namespace mojo {
namespace service {

class AccessPolicyDelegate;

class WindowManagerAccessPolicy : public AccessPolicy {
 public:
  WindowManagerAccessPolicy(ConnectionSpecificId connection_id,
                            AccessPolicyDelegate* delegate);
  virtual ~WindowManagerAccessPolicy();

  // AccessPolicy:
  virtual bool CanRemoveViewFromParent(const ServerView* view) const OVERRIDE;
  virtual bool CanAddView(const ServerView* parent,
                          const ServerView* child) const OVERRIDE;
  virtual bool CanReorderView(const ServerView* view,
                              const ServerView* relative_view,
                              OrderDirection direction) const OVERRIDE;
  virtual bool CanDeleteView(const ServerView* view) const OVERRIDE;
  virtual bool CanGetViewTree(const ServerView* view) const OVERRIDE;
  virtual bool CanDescendIntoViewForViewTree(
      const ServerView* view) const OVERRIDE;
  virtual bool CanEmbed(const ServerView* view) const OVERRIDE;
  virtual bool CanChangeViewVisibility(const ServerView* view) const OVERRIDE;
  virtual bool CanSetViewSurfaceId(const ServerView* view) const OVERRIDE;
  virtual bool CanSetViewBounds(const ServerView* view) const OVERRIDE;
  virtual bool ShouldNotifyOnHierarchyChange(
      const ServerView* view,
      const ServerView** new_parent,
      const ServerView** old_parent) const OVERRIDE;

 private:
  bool IsViewKnown(const ServerView* view) const;

  const ConnectionSpecificId connection_id_;
  AccessPolicyDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerAccessPolicy);
};

}  // namespace service
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_WINDOW_MANAGER_ACCESS_POLICY_H_
