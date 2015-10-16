// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_manager_access_policy.h"

#include "components/mus/ws/access_policy_delegate.h"
#include "components/mus/ws/server_view.h"

namespace mus {

// TODO(sky): document why this differs from default for each case. Maybe want
// to subclass DefaultAccessPolicy.

WindowManagerAccessPolicy::WindowManagerAccessPolicy(
    ConnectionSpecificId connection_id,
    AccessPolicyDelegate* delegate)
    : connection_id_(connection_id), delegate_(delegate) {}

WindowManagerAccessPolicy::~WindowManagerAccessPolicy() {}

bool WindowManagerAccessPolicy::CanRemoveViewFromParent(
    const ServerView* view) const {
  return true;
}

bool WindowManagerAccessPolicy::CanAddView(const ServerView* parent,
                                           const ServerView* child) const {
  return true;
}

bool WindowManagerAccessPolicy::CanReorderView(
    const ServerView* view,
    const ServerView* relative_view,
    mojo::OrderDirection direction) const {
  return true;
}

bool WindowManagerAccessPolicy::CanDeleteView(const ServerView* view) const {
  return view->id().connection_id == connection_id_;
}

bool WindowManagerAccessPolicy::CanGetViewTree(const ServerView* view) const {
  return true;
}

bool WindowManagerAccessPolicy::CanDescendIntoViewForViewTree(
    const ServerView* view) const {
  return true;
}

bool WindowManagerAccessPolicy::CanEmbed(const ServerView* view,
                                         uint32_t policy_bitmask) const {
  return !delegate_->IsRootForAccessPolicy(view->id());
}

bool WindowManagerAccessPolicy::CanChangeViewVisibility(
    const ServerView* view) const {
  // The WindowManager can change the visibility of the root too.
  return view->id().connection_id == connection_id_ ||
         (view->GetRoot() == view);
}

bool WindowManagerAccessPolicy::CanSetWindowSurfaceId(
    const ServerView* view) const {
  if (delegate_->IsViewRootOfAnotherConnectionForAccessPolicy(view))
    return false;
  return view->id().connection_id == connection_id_ ||
         (delegate_->IsRootForAccessPolicy(view->id()));
}

bool WindowManagerAccessPolicy::CanSetViewBounds(const ServerView* view) const {
  return view->id().connection_id == connection_id_;
}

bool WindowManagerAccessPolicy::CanSetViewProperties(
    const ServerView* view) const {
  return view->id().connection_id == connection_id_;
}

bool WindowManagerAccessPolicy::CanSetViewTextInputState(
    const ServerView* view) const {
  return view->id().connection_id == connection_id_;
}

bool WindowManagerAccessPolicy::CanSetFocus(const ServerView* view) const {
  return true;
}

bool WindowManagerAccessPolicy::ShouldNotifyOnHierarchyChange(
    const ServerView* view,
    const ServerView** new_parent,
    const ServerView** old_parent) const {
  // Notify if we've already told the window manager about the view, or if we've
  // already told the window manager about the parent. The later handles the
  // case of a view that wasn't parented to the root getting added to the root.
  return IsViewKnown(view) || (*new_parent && IsViewKnown(*new_parent));
}

const ServerView* WindowManagerAccessPolicy::GetViewForFocusChange(
    const ServerView* focused) {
  return focused;
}

bool WindowManagerAccessPolicy::IsViewKnown(const ServerView* view) const {
  return delegate_->IsViewKnownForAccessPolicy(view);
}

}  // namespace mus
