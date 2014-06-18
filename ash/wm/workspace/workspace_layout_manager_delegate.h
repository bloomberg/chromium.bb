// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_DELEGATE_H_
#define ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_DELEGATE_H_

#include "ash/wm/wm_types.h"

namespace aura {
class Window;
}

namespace ash {
namespace wm {
class WindowState;
}

// A delegate which can be set to create and control a backdrop which gets
// placed below the top level window.
class WorkspaceLayoutManagerDelegate {
 public:
  WorkspaceLayoutManagerDelegate() {}
  virtual ~WorkspaceLayoutManagerDelegate() {}

  // A window got added to the layout.
  virtual void OnWindowAddedToLayout(aura::Window* child) = 0;

  // A window got removed from the layout.
  virtual void OnWindowRemovedFromLayout(aura::Window* child) = 0;

  // The visibility of a window has changed.
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) = 0;

  // The stacking order of a window has changed.
  virtual void OnWindowStackingChanged(aura::Window* window) = 0;

  // A window state type has changed.
  virtual void OnPostWindowStateTypeChange(wm::WindowState* window_state,
                                           wm::WindowStateType old_type) = 0;

  // The work area insets have changed, altering the total available space.
  virtual void OnDisplayWorkAreaInsetsChanged() = 0;
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_DELEGATE_H_
