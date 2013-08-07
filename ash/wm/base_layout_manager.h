// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_BASE_LAYOUT_MANAGER_H_
#define ASH_WM_BASE_LAYOUT_MANAGER_H_

#include <set>

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window_observer.h"
#include "ui/base/events/event_handler.h"
#include "ui/base/ui_base_types.h"

namespace aura {
class RootWindow;
class Window;
}

namespace ash {
namespace internal {

// BaseLayoutManager is the simplest possible implementation for a window
// layout manager. It listens for changes to kShowStateKey and resizes the
// window appropriately.  Subclasses should be sure to invoke the base class
// for adding and removing windows, otherwise show state will not be tracked
// properly.
class ASH_EXPORT BaseLayoutManager
    : public aura::LayoutManager,
      public ash::ShellObserver,
      public aura::WindowObserver,
      public aura::client::ActivationChangeObserver {
 public:
  typedef std::set<aura::Window*> WindowSet;

  explicit BaseLayoutManager(aura::RootWindow* root_window);
  virtual ~BaseLayoutManager();

  const WindowSet& windows() const { return windows_; }

  // Given a |window| and tentative |restore_bounds|, returns new bounds that
  // ensure that at least a few pixels of the screen background are visible
  // outside the edges of the window.
  static gfx::Rect BoundsWithScreenEdgeVisible(aura::Window* window,
                                               const gfx::Rect& restore_bounds);

  // LayoutManager overrides:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  // ash::ShellObserver overrides:
  virtual void OnDisplayWorkAreaInsetsChanged() OVERRIDE;

  // WindowObserver overrides:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;

  // aura::client::ActivationChangeObserver overrides:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

 protected:
  enum AdjustWindowReason {
    ADJUST_WINDOW_DISPLAY_SIZE_CHANGED,
    ADJUST_WINDOW_WORK_AREA_INSETS_CHANGED,
  };

  // Invoked from OnWindowPropertyChanged() if |kShowStateKey| changes.
  virtual void ShowStateChanged(aura::Window* window,
                                ui::WindowShowState last_show_state);

  // Adjusts the window's bounds when the display area changes for given
  // window. This happens when the display size, work area insets or
  // the display on which the window exists has changed.
  // If this is called for a display size change (i.e. |reason|
  // is ADJUST_WINDOW_DISPLAY_SIZE_CHANGED), the non-maximized/non-fullscreen
  // windows are readjusted to make sure the window is completely within the
  // display region. Otherwise, it makes sure at least some parts of the window
  // is on display.
  virtual void AdjustAllWindowsBoundsForWorkAreaChange(
      AdjustWindowReason reason);

  // Adjusts the sizes of the specific window in respond to a screen change or
  // display-area size change.
  virtual void AdjustWindowBoundsForWorkAreaChange(aura::Window* window,
                                                   AdjustWindowReason reason);

  aura::RootWindow* root_window() { return root_window_; }

 private:
  // Update window bounds based on a change in show state.
  void UpdateBoundsFromShowState(aura::Window* window);

  // Set of windows we're listening to.
  WindowSet windows_;

  aura::RootWindow* root_window_;

  DISALLOW_COPY_AND_ASSIGN(BaseLayoutManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_BASE_LAYOUT_MANAGER_H_
