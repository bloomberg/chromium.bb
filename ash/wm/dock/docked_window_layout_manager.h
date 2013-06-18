// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DOCK_DOCKED_WINDOW_LAYOUT_MANAGER_H_
#define ASH_WM_DOCK_DOCKED_WINDOW_LAYOUT_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shell_observer.h"
#include "ash/wm/dock/dock_types.h"
#include "ash/wm/property_util.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window_observer.h"
#include "ui/keyboard/keyboard_controller_observer.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
class Rect;
}

namespace ash {
class Launcher;

namespace internal {
class ShelfLayoutManager;

// DockedWindowLayoutManager is responsible for organizing windows when they are
// docked to the side of a screen. It is associated with a specific container
// window (i.e. kShellWindowId_DockContainer) and controls the layout of any
// windows added to that container.
//
// The constructor takes a |dock_container| argument which is expected to set
// its layout manager to this instance, e.g.:
// dock_container->SetLayoutManager(
//     new DockedWindowLayoutManager(dock_container));

class ASH_EXPORT DockedWindowLayoutManager
    : public aura::LayoutManager,
      public ash::ShellObserver,
      public aura::WindowObserver,
      public aura::client::ActivationChangeObserver,
      public keyboard::KeyboardControllerObserver,
      public ash::ShelfLayoutManagerObserver {
 public:
  explicit DockedWindowLayoutManager(aura::Window* dock_container);
  virtual ~DockedWindowLayoutManager();

  // Called by a DockedWindowResizer to update which window is being dragged.
  void StartDragging(aura::Window* window);
  void FinishDragging();

  // Returns true if a window is touching the side of the screen except 2 cases:
  // when some other windows are already docked on the other side or
  // when launcher (shelf) is aligned on the same side.
  static bool ShouldWindowDock(aura::Window* window,
                               const gfx::Point& location);

  ash::Launcher* launcher() { return launcher_; }
  void SetLauncher(ash::Launcher* launcher);

  // Used to snap docked windows to the side of screen during drag.
  DockedAlignment alignment() const { return alignment_; }

  // Currently dragged window should be able to dock on another screen
  aura::Window* dragged_window() const { return dragged_window_;}

  // aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visibile) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  // ash::ShellObserver:
  virtual void OnShelfAlignmentChanged(aura::RootWindow* root_window) OVERRIDE;

  // aura::WindowObserver:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;

  // aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

  // ShelfLayoutManagerObserver:
  virtual void WillChangeVisibilityState(
      ShelfVisibilityState new_state) OVERRIDE;

 private:
  friend class DockedWindowLayoutManagerTest;
  friend class DockedWindowResizerTest;

  // Minimize / restore window and relayout.
  void MinimizeWindow(aura::Window* window);
  void RestoreWindow(aura::Window* window);

  // Called whenever the window layout might change.
  void Relayout();

  // Called whenever the window stacking order needs to be updated (e.g. focus
  // changes or a window is moved).
  void UpdateStacking(aura::Window* active_window);

  // keyboard::KeyboardControllerObserver:
  virtual void OnKeyboardBoundsChanging(
      const gfx::Rect& keyboard_bounds) OVERRIDE;

  // Parent window associated with this layout manager.
  aura::Window* dock_container_;
  // Protect against recursive calls to Relayout().
  bool in_layout_;
  // The docked window being dragged.
  aura::Window* dragged_window_;
  // The launcher we are observing for launcher icon changes.
  Launcher* launcher_;
  // The shelf layout manager being observed for visibility changes.
  ShelfLayoutManager* shelf_layout_manager_;
  // Tracks the visibility of the shelf. Defaults to false when there is no
  // shelf.
  bool shelf_hidden_;

  // Side of the screen that the dock is positioned at.
  DockedAlignment alignment_;

  DISALLOW_COPY_AND_ASSIGN(DockedWindowLayoutManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_DOCK_DOCKED_WINDOW_LAYOUT_MANAGER_H_
