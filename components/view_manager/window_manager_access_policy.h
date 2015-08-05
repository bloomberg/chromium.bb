// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_WINDOW_MANAGER_ACCESS_POLICY_H_
#define COMPONENTS_VIEW_MANAGER_WINDOW_MANAGER_ACCESS_POLICY_H_

#include "base/basictypes.h"
#include "components/view_manager/access_policy.h"

namespace view_manager {

class AccessPolicyDelegate;

class WindowManagerAccessPolicy : public AccessPolicy {
 public:
  WindowManagerAccessPolicy(mojo::ConnectionSpecificId connection_id,
                            AccessPolicyDelegate* delegate);
  ~WindowManagerAccessPolicy() override;

  // AccessPolicy:
  bool CanRemoveViewFromParent(const ServerView* view) const override;
  bool CanAddView(const ServerView* parent,
                  const ServerView* child) const override;
  bool CanReorderView(const ServerView* view,
                      const ServerView* relative_view,
                      mojo::OrderDirection direction) const override;
  bool CanDeleteView(const ServerView* view) const override;
  bool CanGetViewTree(const ServerView* view) const override;
  bool CanDescendIntoViewForViewTree(const ServerView* view) const override;
  bool CanEmbed(const ServerView* view) const override;
  bool CanChangeViewVisibility(const ServerView* view) const override;
  bool CanSetViewSurfaceId(const ServerView* view) const override;
  bool CanSetViewBounds(const ServerView* view) const override;
  bool CanSetViewProperties(const ServerView* view) const override;
  bool CanSetViewTextInputState(const ServerView* view) const override;
  bool CanSetFocus(const ServerView* view) const override;
  bool ShouldNotifyOnHierarchyChange(
      const ServerView* view,
      const ServerView** new_parent,
      const ServerView** old_parent) const override;
  const ServerView* GetViewForFocusChange(const ServerView* focused) override;

 private:
  bool IsViewKnown(const ServerView* view) const;

  const mojo::ConnectionSpecificId connection_id_;
  AccessPolicyDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerAccessPolicy);
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_WINDOW_MANAGER_ACCESS_POLICY_H_
