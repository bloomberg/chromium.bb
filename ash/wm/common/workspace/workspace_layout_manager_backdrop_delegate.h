// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_BACKDROP_DELEGATE_H_
#define ASH_WM_COMMON_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_BACKDROP_DELEGATE_H_

#include "ash/wm/common/ash_wm_common_export.h"
#include "ash/wm/common/wm_types.h"

namespace ash {
namespace wm {
class WindowState;
class WmWindow;
}

// A delegate which can be set to create and control a backdrop which gets
// placed below the top level window.
class ASH_WM_COMMON_EXPORT WorkspaceLayoutManagerBackdropDelegate {
 public:
  virtual ~WorkspaceLayoutManagerBackdropDelegate() {}

  // A window got added to the layout.
  virtual void OnWindowAddedToLayout(wm::WmWindow* child) = 0;

  // A window got removed from the layout.
  virtual void OnWindowRemovedFromLayout(wm::WmWindow* child) = 0;

  // The visibility of a window has changed.
  virtual void OnChildWindowVisibilityChanged(wm::WmWindow* child,
                                              bool visible) = 0;

  // The stacking order of a window has changed.
  virtual void OnWindowStackingChanged(wm::WmWindow* window) = 0;

  // A window state type has changed.
  virtual void OnPostWindowStateTypeChange(wm::WindowState* window_state,
                                           wm::WindowStateType old_type) = 0;

  // The work area insets have changed, altering the total available space.
  virtual void OnDisplayWorkAreaInsetsChanged() = 0;
};

}  // namespace ash

#endif  // ASH_WM_COMMON_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_BACKDROP_DELEGATE_H_
