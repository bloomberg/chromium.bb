// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_
#define ASH_COMMON_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_

#include <memory>
#include <set>

#include "ash/ash_export.h"
#include "ash/common/shell_observer.h"
#include "ash/common/wm/window_state_observer.h"
#include "ash/common/wm/wm_types.h"
#include "ash/common/wm_activation_observer.h"
#include "ash/common/wm_layout_manager.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/keyboard_controller_observer.h"

namespace ash {

class RootWindowController;
class WmShell;
class WmWindow;
class WorkspaceLayoutManagerBackdropDelegate;

namespace wm {
class WMEvent;
}

// LayoutManager used on the window created for a workspace.
class ASH_EXPORT WorkspaceLayoutManager
    : public WmLayoutManager,
      public aura::WindowObserver,
      public WmActivationObserver,
      public keyboard::KeyboardControllerObserver,
      public display::DisplayObserver,
      public ShellObserver,
      public wm::WindowStateObserver {
 public:
  // |window| is the container for this layout manager.
  explicit WorkspaceLayoutManager(WmWindow* window);
  ~WorkspaceLayoutManager() override;

  // A delegate which can be set to add a backdrop behind the top most visible
  // window. With the call the ownership of the delegate will be transferred to
  // the WorkspaceLayoutManager.
  void SetMaximizeBackdropDelegate(
      std::unique_ptr<WorkspaceLayoutManagerBackdropDelegate> delegate);

  // Overridden from WmLayoutManager:
  void OnWindowResized() override;
  void OnWindowAddedToLayout(WmWindow* child) override;
  void OnWillRemoveWindowFromLayout(WmWindow* child) override;
  void OnWindowRemovedFromLayout(WmWindow* child) override;
  void OnChildWindowVisibilityChanged(WmWindow* child, bool visibile) override;
  void SetChildBounds(WmWindow* child,
                      const gfx::Rect& requested_bounds) override;

  // Overriden from aura::WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;

  // WmActivationObserver overrides:
  void OnWindowActivated(WmWindow* gained_active,
                         WmWindow* lost_active) override;

  // keyboard::KeyboardControllerObserver overrides:
  void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) override;
  void OnKeyboardClosed() override;

  // WindowStateObserver overrides:
  void OnPostWindowStateTypeChange(wm::WindowState* window_state,
                                   wm::WindowStateType old_type) override;

  // display::DisplayObserver overrides:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // ShellObserver overrides:
  void OnFullscreenStateChanged(bool is_fullscreen,
                                WmWindow* root_window) override;
  void OnPinnedStateChanged(WmWindow* pinned_window) override;

 private:
  typedef std::set<WmWindow*> WindowSet;

  // Adjusts the bounds of all managed windows when the display area changes.
  // This happens when the display size, work area insets has changed.
  // If this is called for a display size change (i.e. |event|
  // is DISPLAY_RESIZED), the non-maximized/non-fullscreen
  // windows are readjusted to make sure the window is completely within the
  // display region. Otherwise, it makes sure at least some parts of the window
  // is on display.
  void AdjustAllWindowsBoundsForWorkAreaChange(const wm::WMEvent* event);

  // Updates the visibility state of the shelf.
  void UpdateShelfVisibility();

  // Updates the fullscreen state of the workspace and notifies Shell if it
  // has changed.
  void UpdateFullscreenState();

  // Updates the always-on-top state for windows managed by this layout
  // manager.
  void UpdateAlwaysOnTop(WmWindow* window_on_top);

  WmWindow* window_;
  WmWindow* root_window_;
  RootWindowController* root_window_controller_;
  WmShell* shell_;

  // Set of windows we're listening to.
  WindowSet windows_;

  // The work area in the coordinates of |window_|.
  gfx::Rect work_area_in_parent_;

  // True if this workspace is currently in fullscreen mode.
  bool is_fullscreen_;

  // A window which covers the full container and which gets inserted behind the
  // topmost visible window.
  std::unique_ptr<WorkspaceLayoutManagerBackdropDelegate> backdrop_delegate_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceLayoutManager);
};

}  // namespace ash

#endif  // ASH_COMMON_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_
