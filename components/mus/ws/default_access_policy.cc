// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/default_access_policy.h"

#include "components/mus/ws/access_policy_delegate.h"
#include "components/mus/ws/server_window.h"

namespace mus {

DefaultAccessPolicy::DefaultAccessPolicy(ConnectionSpecificId connection_id,
                                         AccessPolicyDelegate* delegate)
    : connection_id_(connection_id), delegate_(delegate) {}

DefaultAccessPolicy::~DefaultAccessPolicy() {}

bool DefaultAccessPolicy::CanRemoveWindowFromParent(
    const ServerWindow* window) const {
  if (!WasCreatedByThisConnection(window))
    return false;  // Can only unparent windows we created.

  return delegate_->IsRootForAccessPolicy(window->parent()->id()) ||
         WasCreatedByThisConnection(window->parent());
}

bool DefaultAccessPolicy::CanAddWindow(const ServerWindow* parent,
                                       const ServerWindow* child) const {
  return WasCreatedByThisConnection(child) &&
         (delegate_->IsRootForAccessPolicy(parent->id()) ||
          (WasCreatedByThisConnection(parent) &&
           !delegate_->IsWindowRootOfAnotherConnectionForAccessPolicy(parent)));
}

bool DefaultAccessPolicy::CanReorderWindow(
    const ServerWindow* window,
    const ServerWindow* relative_window,
    mojom::OrderDirection direction) const {
  return WasCreatedByThisConnection(window) &&
         WasCreatedByThisConnection(relative_window);
}

bool DefaultAccessPolicy::CanDeleteWindow(const ServerWindow* window) const {
  return WasCreatedByThisConnection(window);
}

bool DefaultAccessPolicy::CanGetWindowTree(const ServerWindow* window) const {
  return WasCreatedByThisConnection(window) ||
         delegate_->IsRootForAccessPolicy(window->id()) ||
         IsDescendantOfEmbedRoot(window);
}

bool DefaultAccessPolicy::CanDescendIntoWindowForWindowTree(
    const ServerWindow* window) const {
  return (WasCreatedByThisConnection(window) &&
          !delegate_->IsWindowRootOfAnotherConnectionForAccessPolicy(window)) ||
         delegate_->IsRootForAccessPolicy(window->id()) ||
         delegate_->IsDescendantOfEmbedRoot(window);
}

bool DefaultAccessPolicy::CanEmbed(const ServerWindow* window,
                                   uint32_t policy_bitmask) const {
  if (policy_bitmask != mojom::WindowTree::ACCESS_POLICY_DEFAULT)
    return false;
  return WasCreatedByThisConnection(window) ||
         (delegate_->IsWindowKnownForAccessPolicy(window) &&
          IsDescendantOfEmbedRoot(window) &&
          !delegate_->IsRootForAccessPolicy(window->id()));
}

bool DefaultAccessPolicy::CanChangeWindowVisibility(
    const ServerWindow* window) const {
  return WasCreatedByThisConnection(window) ||
         delegate_->IsRootForAccessPolicy(window->id());
}

bool DefaultAccessPolicy::CanSetWindowSurfaceId(
    const ServerWindow* window) const {
  // Once a window embeds another app, the embedder app is no longer able to
  // call SetWindowSurfaceId() - this ability is transferred to the embedded
  // app.
  if (delegate_->IsWindowRootOfAnotherConnectionForAccessPolicy(window))
    return false;
  return WasCreatedByThisConnection(window) ||
         delegate_->IsRootForAccessPolicy(window->id());
}

bool DefaultAccessPolicy::CanSetWindowBounds(const ServerWindow* window) const {
  return WasCreatedByThisConnection(window);
}

bool DefaultAccessPolicy::CanSetWindowProperties(
    const ServerWindow* window) const {
  return WasCreatedByThisConnection(window);
}

bool DefaultAccessPolicy::CanSetWindowTextInputState(
    const ServerWindow* window) const {
  return WasCreatedByThisConnection(window) ||
         delegate_->IsRootForAccessPolicy(window->id());
}

bool DefaultAccessPolicy::CanSetFocus(const ServerWindow* window) const {
  return WasCreatedByThisConnection(window) ||
         delegate_->IsRootForAccessPolicy(window->id());
}

bool DefaultAccessPolicy::CanSetClientArea(const ServerWindow* window) const {
  return WasCreatedByThisConnection(window) ||
         delegate_->IsRootForAccessPolicy(window->id());
}

bool DefaultAccessPolicy::ShouldNotifyOnHierarchyChange(
    const ServerWindow* window,
    const ServerWindow** new_parent,
    const ServerWindow** old_parent) const {
  if (!WasCreatedByThisConnection(window) && !IsDescendantOfEmbedRoot(window) &&
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

const ServerWindow* DefaultAccessPolicy::GetWindowForFocusChange(
    const ServerWindow* focused) {
  if (WasCreatedByThisConnection(focused) ||
      delegate_->IsRootForAccessPolicy(focused->id()))
    return focused;
  return nullptr;
}

bool DefaultAccessPolicy::WasCreatedByThisConnection(
    const ServerWindow* window) const {
  return window->id().connection_id == connection_id_;
}

bool DefaultAccessPolicy::IsDescendantOfEmbedRoot(
    const ServerWindow* window) const {
  return delegate_->IsDescendantOfEmbedRoot(window);
}

}  // namespace mus
