// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_

#include <set>

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "ash/wm/window_state_observer.h"
#include "ash/wm/wm_types.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/rect.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class RootWindow;
class Window;
}

namespace ui {
class Layer;
}

namespace ash {
class ShelfLayoutManager;
class WorkspaceLayoutManagerDelegate;

namespace wm {
class WindowState;
class WMEvent;
}

// LayoutManager used on the window created for a workspace.
class ASH_EXPORT WorkspaceLayoutManager
    : public aura::LayoutManager,
      public aura::WindowObserver,
      public aura::client::ActivationChangeObserver,
      public keyboard::KeyboardControllerObserver,
      public ShellObserver,
      public wm::WindowStateObserver {
 public:
  explicit WorkspaceLayoutManager(aura::Window* window);
  virtual ~WorkspaceLayoutManager();

  void SetShelf(ShelfLayoutManager* shelf);

  // A delegate which can be set to add a backdrop behind the top most visible
  // window. With the call the ownership of the delegate will be transferred to
  // the WorkspaceLayoutManager.
  void SetMaximizeBackdropDelegate(
      scoped_ptr<WorkspaceLayoutManagerDelegate> delegate);

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE {}
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visibile) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  // ash::ShellObserver overrides:
  virtual void OnDisplayWorkAreaInsetsChanged() OVERRIDE;

  // Overriden from WindowObserver:
  virtual void OnWindowHierarchyChanged(
      const WindowObserver::HierarchyChangeParams& params) OVERRIDE;
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;
  virtual void OnWindowStackingChanged(aura::Window* window) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;

  // aura::client::ActivationChangeObserver overrides:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

  // keyboard::KeyboardControllerObserver overrides:
  virtual void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) OVERRIDE;

  // WindowStateObserver overrides:
  virtual void OnPostWindowStateTypeChange(
      wm::WindowState* window_state,
      wm::WindowStateType old_type) OVERRIDE;

 private:
  typedef std::set<aura::Window*> WindowSet;

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
  void SetChildBoundsAnimated(aura::Window* child, const gfx::Rect& bounds);

  ShelfLayoutManager* shelf_;
  aura::Window* window_;
  aura::Window* root_window_;

  // Set of windows we're listening to.
  WindowSet windows_;

  // The work area in the coordinates of |window_|.
  gfx::Rect work_area_in_parent_;

  // True if this workspace is currently in fullscreen mode.
  bool is_fullscreen_;

  // A window which covers the full container and which gets inserted behind the
  // topmost visible window.
  scoped_ptr<WorkspaceLayoutManagerDelegate> backdrop_delegate_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceLayoutManager);
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_
