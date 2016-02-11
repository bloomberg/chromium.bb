// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_manager_access_policy.h"

#include "components/mus/ws/access_policy_delegate.h"
#include "components/mus/ws/server_window.h"

namespace mus {

namespace ws {

// TODO(sky): document why this differs from default for each case. Maybe want
// to subclass DefaultAccessPolicy.

WindowManagerAccessPolicy::WindowManagerAccessPolicy(
    ConnectionSpecificId connection_id,
    AccessPolicyDelegate* delegate)
    : connection_id_(connection_id), delegate_(delegate) {}

WindowManagerAccessPolicy::~WindowManagerAccessPolicy() {}

bool WindowManagerAccessPolicy::CanRemoveWindowFromParent(
    const ServerWindow* window) const {
  return true;
}

bool WindowManagerAccessPolicy::CanAddWindow(const ServerWindow* parent,
                                             const ServerWindow* child) const {
  return true;
}

bool WindowManagerAccessPolicy::CanAddTransientWindow(
    const ServerWindow* parent,
    const ServerWindow* child) const {
  return true;
}

bool WindowManagerAccessPolicy::CanRemoveTransientWindowFromParent(
    const ServerWindow* window) const {
  return true;
}

bool WindowManagerAccessPolicy::CanReorderWindow(
    const ServerWindow* window,
    const ServerWindow* relative_window,
    mojom::OrderDirection direction) const {
  return true;
}

bool WindowManagerAccessPolicy::CanDeleteWindow(
    const ServerWindow* window) const {
  return window->id().connection_id == connection_id_;
}

bool WindowManagerAccessPolicy::CanGetWindowTree(
    const ServerWindow* window) const {
  return true;
}

bool WindowManagerAccessPolicy::CanDescendIntoWindowForWindowTree(
    const ServerWindow* window) const {
  return true;
}

bool WindowManagerAccessPolicy::CanEmbed(const ServerWindow* window,
                                         uint32_t policy_bitmask) const {
  return !delegate_->HasRootForAccessPolicy(window);
}

bool WindowManagerAccessPolicy::CanChangeWindowVisibility(
    const ServerWindow* window) const {
  // The WindowManager can change the visibility of the root too.
  return window->id().connection_id == connection_id_ ||
         (window->GetRoot() == window);
}

bool WindowManagerAccessPolicy::CanSetWindowSurface(
    const ServerWindow* window,
    mus::mojom::SurfaceType surface_type) const {
  if (surface_type == mojom::SurfaceType::UNDERLAY)
    return window->id().connection_id == connection_id_;

  if (delegate_->IsWindowRootOfAnotherConnectionForAccessPolicy(window))
    return false;
  return window->id().connection_id == connection_id_ ||
         (delegate_->HasRootForAccessPolicy(window));
}

bool WindowManagerAccessPolicy::CanSetWindowBounds(
    const ServerWindow* window) const {
  return window->id().connection_id == connection_id_;
}

bool WindowManagerAccessPolicy::CanSetWindowProperties(
    const ServerWindow* window) const {
  return window->id().connection_id == connection_id_;
}

bool WindowManagerAccessPolicy::CanSetWindowTextInputState(
    const ServerWindow* window) const {
  return window->id().connection_id == connection_id_;
}

bool WindowManagerAccessPolicy::CanSetCapture(
    const ServerWindow* window) const {
  return window->id().connection_id == connection_id_;
}

bool WindowManagerAccessPolicy::CanSetFocus(const ServerWindow* window) const {
  return true;
}

bool WindowManagerAccessPolicy::CanSetClientArea(
    const ServerWindow* window) const {
  return window->id().connection_id == connection_id_ ||
         delegate_->HasRootForAccessPolicy(window);
}

bool WindowManagerAccessPolicy::CanSetCursorProperties(
    const ServerWindow* window) const {
  return window->id().connection_id == connection_id_ ||
         delegate_->HasRootForAccessPolicy(window);
}

bool WindowManagerAccessPolicy::ShouldNotifyOnHierarchyChange(
    const ServerWindow* window,
    const ServerWindow** new_parent,
    const ServerWindow** old_parent) const {
  // Notify if we've already told the window manager about the window, or if
  // we've
  // already told the window manager about the parent. The later handles the
  // case of a window that wasn't parented to the root getting added to the
  // root.
  return IsWindowKnown(window) || (*new_parent && IsWindowKnown(*new_parent));
}

bool WindowManagerAccessPolicy::CanSetWindowManager() const {
  return true;
}

const ServerWindow* WindowManagerAccessPolicy::GetWindowForFocusChange(
    const ServerWindow* focused) {
  return focused;
}

bool WindowManagerAccessPolicy::IsWindowKnown(
    const ServerWindow* window) const {
  return delegate_->IsWindowKnownForAccessPolicy(window);
}

}  // namespace ws

}  // namespace mus
