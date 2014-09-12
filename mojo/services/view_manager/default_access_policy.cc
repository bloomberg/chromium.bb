// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/default_access_policy.h"

#include "mojo/services/view_manager/access_policy_delegate.h"
#include "mojo/services/view_manager/server_view.h"

namespace mojo {
namespace service {

DefaultAccessPolicy::DefaultAccessPolicy(ConnectionSpecificId connection_id,
                                         AccessPolicyDelegate* delegate)
    : connection_id_(connection_id),
      delegate_(delegate) {
}

DefaultAccessPolicy::~DefaultAccessPolicy() {
}

bool DefaultAccessPolicy::CanRemoveViewFromParent(
    const ServerView* view) const {
  if (!WasCreatedByThisConnection(view))
    return false;  // Can only unparent views we created.

  return IsViewInRoots(view->parent()) ||
         WasCreatedByThisConnection(view->parent());
}

bool DefaultAccessPolicy::CanAddView(const ServerView* parent,
                                     const ServerView* child) const {
  return WasCreatedByThisConnection(child) &&
         (IsViewInRoots(parent) ||
          (WasCreatedByThisConnection(parent) &&
           !delegate_->IsViewRootOfAnotherConnectionForAccessPolicy(parent)));
}

bool DefaultAccessPolicy::CanReorderView(const ServerView* view,
                                         const ServerView* relative_view,
                                         OrderDirection direction) const {
  return WasCreatedByThisConnection(view) &&
         WasCreatedByThisConnection(relative_view);
}

bool DefaultAccessPolicy::CanDeleteView(const ServerView* view) const {
  return WasCreatedByThisConnection(view);
}

bool DefaultAccessPolicy::CanGetViewTree(const ServerView* view) const {
  return WasCreatedByThisConnection(view) || IsViewInRoots(view);
}

bool DefaultAccessPolicy::CanDescendIntoViewForViewTree(
    const ServerView* view) const {
  return WasCreatedByThisConnection(view) &&
         !delegate_->IsViewRootOfAnotherConnectionForAccessPolicy(view);
}

bool DefaultAccessPolicy::CanEmbed(const ServerView* view) const {
  return WasCreatedByThisConnection(view);
}

bool DefaultAccessPolicy::CanChangeViewVisibility(
    const ServerView* view) const {
  return WasCreatedByThisConnection(view) || IsViewInRoots(view);
}

bool DefaultAccessPolicy::CanSetViewSurfaceId(const ServerView* view) const {
  // Once a view embeds another app, the embedder app is no longer able to
  // call SetViewSurfaceId() - this ability is transferred to the embedded app.
  if (delegate_->IsViewRootOfAnotherConnectionForAccessPolicy(view))
    return false;
  return WasCreatedByThisConnection(view) || IsViewInRoots(view);
}

bool DefaultAccessPolicy::CanSetViewBounds(const ServerView* view) const {
  return WasCreatedByThisConnection(view);
}

bool DefaultAccessPolicy::ShouldNotifyOnHierarchyChange(
    const ServerView* view,
    const ServerView** new_parent,
    const ServerView** old_parent) const {
  if (!WasCreatedByThisConnection(view))
    return false;

  if (*new_parent && !WasCreatedByThisConnection(*new_parent) &&
      !IsViewInRoots(*new_parent)) {
    *new_parent = NULL;
  }

  if (*old_parent && !WasCreatedByThisConnection(*old_parent) &&
      !IsViewInRoots(*old_parent)) {
    *old_parent = NULL;
  }
  return true;
}

bool DefaultAccessPolicy::IsViewInRoots(const ServerView* view) const {
  return delegate_->GetRootsForAccessPolicy().count(
             ViewIdToTransportId(view->id())) > 0;
}

}  // namespace service
}  // namespace mojo
