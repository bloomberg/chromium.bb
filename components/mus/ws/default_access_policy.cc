// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/default_access_policy.h"

#include "components/mus/ws/access_policy_delegate.h"
#include "components/mus/ws/server_view.h"

namespace mus {

DefaultAccessPolicy::DefaultAccessPolicy(ConnectionSpecificId connection_id,
                                         AccessPolicyDelegate* delegate)
    : connection_id_(connection_id), delegate_(delegate) {}

DefaultAccessPolicy::~DefaultAccessPolicy() {}

bool DefaultAccessPolicy::CanRemoveViewFromParent(
    const ServerView* view) const {
  if (!WasCreatedByThisConnection(view))
    return false;  // Can only unparent views we created.

  return delegate_->IsRootForAccessPolicy(view->parent()->id()) ||
         WasCreatedByThisConnection(view->parent());
}

bool DefaultAccessPolicy::CanAddView(const ServerView* parent,
                                     const ServerView* child) const {
  return WasCreatedByThisConnection(child) &&
         (delegate_->IsRootForAccessPolicy(parent->id()) ||
          (WasCreatedByThisConnection(parent) &&
           !delegate_->IsViewRootOfAnotherConnectionForAccessPolicy(parent)));
}

bool DefaultAccessPolicy::CanReorderView(const ServerView* view,
                                         const ServerView* relative_view,
                                         mojo::OrderDirection direction) const {
  return WasCreatedByThisConnection(view) &&
         WasCreatedByThisConnection(relative_view);
}

bool DefaultAccessPolicy::CanDeleteView(const ServerView* view) const {
  return WasCreatedByThisConnection(view);
}

bool DefaultAccessPolicy::CanGetViewTree(const ServerView* view) const {
  return WasCreatedByThisConnection(view) ||
         delegate_->IsRootForAccessPolicy(view->id()) ||
         IsDescendantOfEmbedRoot(view);
}

bool DefaultAccessPolicy::CanDescendIntoViewForViewTree(
    const ServerView* view) const {
  return (WasCreatedByThisConnection(view) &&
          !delegate_->IsViewRootOfAnotherConnectionForAccessPolicy(view)) ||
         delegate_->IsRootForAccessPolicy(view->id()) ||
         delegate_->IsDescendantOfEmbedRoot(view);
}

bool DefaultAccessPolicy::CanEmbed(const ServerView* view,
                                   uint32_t policy_bitmask) const {
  if (policy_bitmask != mojo::ViewTree::ACCESS_POLICY_DEFAULT)
    return false;
  return WasCreatedByThisConnection(view) ||
         (delegate_->IsViewKnownForAccessPolicy(view) &&
          IsDescendantOfEmbedRoot(view) &&
          !delegate_->IsRootForAccessPolicy(view->id()));
}

bool DefaultAccessPolicy::CanChangeViewVisibility(
    const ServerView* view) const {
  return WasCreatedByThisConnection(view) ||
         delegate_->IsRootForAccessPolicy(view->id());
}

bool DefaultAccessPolicy::CanSetWindowSurfaceId(const ServerView* view) const {
  // Once a view embeds another app, the embedder app is no longer able to
  // call SetWindowSurfaceId() - this ability is transferred to the embedded
  // app.
  if (delegate_->IsViewRootOfAnotherConnectionForAccessPolicy(view))
    return false;
  return WasCreatedByThisConnection(view) ||
         delegate_->IsRootForAccessPolicy(view->id());
}

bool DefaultAccessPolicy::CanSetViewBounds(const ServerView* view) const {
  return WasCreatedByThisConnection(view);
}

bool DefaultAccessPolicy::CanSetViewProperties(const ServerView* view) const {
  return WasCreatedByThisConnection(view);
}

bool DefaultAccessPolicy::CanSetViewTextInputState(
    const ServerView* view) const {
  return WasCreatedByThisConnection(view) ||
         delegate_->IsRootForAccessPolicy(view->id());
}

bool DefaultAccessPolicy::CanSetFocus(const ServerView* view) const {
  return WasCreatedByThisConnection(view) ||
         delegate_->IsRootForAccessPolicy(view->id());
}

bool DefaultAccessPolicy::ShouldNotifyOnHierarchyChange(
    const ServerView* view,
    const ServerView** new_parent,
    const ServerView** old_parent) const {
  if (!WasCreatedByThisConnection(view) && !IsDescendantOfEmbedRoot(view) &&
      (!*new_parent || !IsDescendantOfEmbedRoot(*new_parent)) &&
      (!*old_parent || !IsDescendantOfEmbedRoot(*old_parent))) {
    return false;
  }

  if (*new_parent && !WasCreatedByThisConnection(*new_parent) &&
      !delegate_->IsRootForAccessPolicy((*new_parent)->id()) &&
      !delegate_->IsDescendantOfEmbedRoot(*new_parent)) {
    *new_parent = nullptr;
  }

  if (*old_parent && !WasCreatedByThisConnection(*old_parent) &&
      !delegate_->IsRootForAccessPolicy((*old_parent)->id()) &&
      !delegate_->IsDescendantOfEmbedRoot(*new_parent)) {
    *old_parent = nullptr;
  }
  return true;
}

const ServerView* DefaultAccessPolicy::GetViewForFocusChange(
    const ServerView* focused) {
  if (WasCreatedByThisConnection(focused) ||
      delegate_->IsRootForAccessPolicy(focused->id()))
    return focused;
  return nullptr;
}

bool DefaultAccessPolicy::WasCreatedByThisConnection(
    const ServerView* view) const {
  return view->id().connection_id == connection_id_;
}

bool DefaultAccessPolicy::IsDescendantOfEmbedRoot(
    const ServerView* view) const {
  return delegate_->IsDescendantOfEmbedRoot(view);
}

}  // namespace mus
