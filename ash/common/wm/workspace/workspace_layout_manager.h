// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_
#define ASH_COMMON_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_

#include <memory>
#include <set>

#include "ash/ash_export.h"
#include "ash/common/wm/window_state_observer.h"
#include "ash/common/wm/wm_types.h"
#include "ash/common/wm_activation_observer.h"
#include "ash/common/wm_layout_manager.h"
#include "ash/common/wm_root_window_controller_observer.h"
#include "ash/common/wm_window_observer.h"
#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/keyboard_controller_observer.h"

namespace ash {
class WmShell;
class WmRootWindowController;
class WorkspaceLayoutManagerBackdropDelegate;

namespace wm {
class WorkspaceLayoutManagerDelegate;
class WMEvent;
}

// LayoutManager used on the window created for a workspace.
class ASH_EXPORT WorkspaceLayoutManager
    : public WmLayoutManager,
      public WmWindowObserver,
      public WmActivationObserver,
      public keyboard::KeyboardControllerObserver,
      public WmRootWindowControllerObserver,
      public wm::WindowStateObserver {
 public:
  WorkspaceLayoutManager(
      WmWindow* window,
      std::unique_ptr<wm::WorkspaceLayoutManagerDelegate> delegate);

  ~WorkspaceLayoutManager() override;

  void DeleteDelegate();

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

  // WmRootWindowControllerObserver overrides:
  void OnWorkAreaChanged() override;
  void OnFullscreenStateChanged(bool is_fullscreen) override;

  // Overriden from WmWindowObserver:
  void OnWindowTreeChanged(
      WmWindow* window,
      const WmWindowObserver::TreeChangeParams& params) override;
  void OnWindowPropertyChanged(WmWindow* window,
                               WmWindowProperty property) override;
  void OnWindowStackingChanged(WmWindow* window) override;
  void OnWindowDestroying(WmWindow* window) override;
  void OnWindowBoundsChanged(WmWindow* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;

  // WmActivationObserver overrides:
  void OnWindowActivated(WmWindow* gained_active,
                         WmWindow* lost_active) override;

  // keyboard::KeyboardControllerObserver overrides:
  void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) override;

  // WindowStateObserver overrides:
  void OnPostWindowStateTypeChange(wm::WindowState* window_state,
                                   wm::WindowStateType old_type) override;

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

  // Updates the bounds of the window for a stte type change from
  // |old_show_type|.
  void UpdateBoundsFromStateType(wm::WindowState* window_state,
                                 wm::WindowStateType old_state_type);

  // If |window_state| is maximized or fullscreen the bounds of the
  // window are set and true is returned. Does nothing otherwise.
  bool SetMaximizedOrFullscreenBounds(wm::WindowState* window_state);

  // Animates the window bounds to |bounds|.
  void SetChildBoundsAnimated(WmWindow* child, const gfx::Rect& bounds);

  WmWindow* window_;
  WmWindow* root_window_;
  WmRootWindowController* root_window_controller_;
  WmShell* shell_;

  std::unique_ptr<wm::WorkspaceLayoutManagerDelegate> delegate_;

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
