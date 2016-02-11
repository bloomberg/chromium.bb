// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/default_access_policy.h"

#include "components/mus/ws/access_policy_delegate.h"
#include "components/mus/ws/server_window.h"

namespace mus {

namespace ws {

DefaultAccessPolicy::DefaultAccessPolicy(ConnectionSpecificId connection_id,
                                         AccessPolicyDelegate* delegate)
    : connection_id_(connection_id), delegate_(delegate) {}

DefaultAccessPolicy::~DefaultAccessPolicy() {}

bool DefaultAccessPolicy::CanRemoveWindowFromParent(
    const ServerWindow* window) const {
  if (!WasCreatedByThisConnection(window))
    return false;  // Can only unparent windows we created.

  return delegate_->HasRootForAccessPolicy(window->parent()) ||
         WasCreatedByThisConnection(window->parent());
}

bool DefaultAccessPolicy::CanAddWindow(const ServerWindow* parent,
                                       const ServerWindow* child) const {
  return WasCreatedByThisConnection(child) &&
         (delegate_->HasRootForAccessPolicy(parent) ||
          (WasCreatedByThisConnection(parent) &&
           !delegate_->IsWindowRootOfAnotherConnectionForAccessPolicy(parent)));
}

bool DefaultAccessPolicy::CanAddTransientWindow(
    const ServerWindow* parent,
    const ServerWindow* child) const {
  return (delegate_->HasRootForAccessPolicy(child) ||
          WasCreatedByThisConnection(child)) &&
         (delegate_->HasRootForAccessPolicy(parent) ||
          WasCreatedByThisConnection(parent));
}

bool DefaultAccessPolicy::CanRemoveTransientWindowFromParent(
    const ServerWindow* window) const {
  return (delegate_->HasRootForAccessPolicy(window) ||
          WasCreatedByThisConnection(window)) &&
         (delegate_->HasRootForAccessPolicy(window->transient_parent()) ||
          WasCreatedByThisConnection(window->transient_parent()));
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
         delegate_->HasRootForAccessPolicy(window) ||
         IsDescendantOfEmbedRoot(window);
}

bool DefaultAccessPolicy::CanDescendIntoWindowForWindowTree(
    const ServerWindow* window) const {
  return (WasCreatedByThisConnection(window) &&
          !delegate_->IsWindowRootOfAnotherConnectionForAccessPolicy(window)) ||
         delegate_->HasRootForAccessPolicy(window) ||
         delegate_->IsDescendantOfEmbedRoot(window);
}

bool DefaultAccessPolicy::CanEmbed(const ServerWindow* window,
                                   uint32_t policy_bitmask) const {
  if (policy_bitmask != mojom::WindowTree::kAccessPolicyDefault)
    return false;
  return WasCreatedByThisConnection(window) ||
         (delegate_->IsWindowKnownForAccessPolicy(window) &&
          IsDescendantOfEmbedRoot(window) &&
          !delegate_->HasRootForAccessPolicy(window));
}

bool DefaultAccessPolicy::CanChangeWindowVisibility(
    const ServerWindow* window) const {
  return WasCreatedByThisConnection(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool DefaultAccessPolicy::CanSetWindowSurface(
    const ServerWindow* window,
    mojom::SurfaceType surface_type) const {
  if (surface_type == mojom::SurfaceType::UNDERLAY)
    return WasCreatedByThisConnection(window);

  // Once a window embeds another app, the embedder app is no longer able to
  // call SetWindowSurfaceId() - this ability is transferred to the embedded
  // app.
  if (delegate_->IsWindowRootOfAnotherConnectionForAccessPolicy(window))
    return false;
  return WasCreatedByThisConnection(window) ||
         delegate_->HasRootForAccessPolicy(window);
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
         delegate_->HasRootForAccessPolicy(window);
}

bool DefaultAccessPolicy::CanSetCapture(const ServerWindow* window) const {
  return WasCreatedByThisConnection(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool DefaultAccessPolicy::CanSetFocus(const ServerWindow* window) const {
  return WasCreatedByThisConnection(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool DefaultAccessPolicy::CanSetClientArea(const ServerWindow* window) const {
  return WasCreatedByThisConnection(window) ||
         delegate_->HasRootForAccessPolicy(window);
}

bool DefaultAccessPolicy::CanSetCursorProperties(
    const ServerWindow* window) const {
  return WasCreatedByThisConnection(window) ||
         delegate_->HasRootForAccessPolicy(window);
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
      !delegate_->HasRootForAccessPolicy((*new_parent)) &&
      !delegate_->IsDescendantOfEmbedRoot(*new_parent)) {
    *new_parent = nullptr;
  }

  if (*old_parent && !WasCreatedByThisConnection(*old_parent) &&
      !delegate_->HasRootForAccessPolicy((*old_parent)) &&
      !delegate_->IsDescendantOfEmbedRoot(*new_parent)) {
    *old_parent = nullptr;
  }
  return true;
}

const ServerWindow* DefaultAccessPolicy::GetWindowForFocusChange(
    const ServerWindow* focused) {
  if (WasCreatedByThisConnection(focused) ||
      delegate_->HasRootForAccessPolicy(focused))
    return focused;
  return nullptr;
}

bool DefaultAccessPolicy::CanSetWindowManager() const {
  return false;
}

bool DefaultAccessPolicy::WasCreatedByThisConnection(
    const ServerWindow* window) const {
  return window->id().connection_id == connection_id_;
}

bool DefaultAccessPolicy::IsDescendantOfEmbedRoot(
    const ServerWindow* window) const {
  return delegate_->IsDescendantOfEmbedRoot(window);
}

}  // namespace ws

}  // namespace mus
