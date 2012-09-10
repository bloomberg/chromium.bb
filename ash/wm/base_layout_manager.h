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
#include "ui/aura/layout_manager.h"
#include "ui/aura/root_window_observer.h"
#include "ui/base/ui_base_types.h"
#include "ui/aura/window_observer.h"

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
class ASH_EXPORT BaseLayoutManager : public aura::LayoutManager,
                                     public aura::RootWindowObserver,
                                     public ash::ShellObserver,
                                     public aura::WindowObserver {
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

  // RootWindowObserver overrides:
  virtual void OnRootWindowResized(const aura::RootWindow* root,
                                   const gfx::Size& old_size) OVERRIDE;

  // ash::ShellObserver overrides:
  virtual void OnDisplayWorkAreaInsetsChanged() OVERRIDE;

  // WindowObserver overrides:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

 protected:
  // Invoked from OnWindowPropertyChanged() if |kShowStateKey| changes.
  virtual void ShowStateChanged(aura::Window* window,
                                ui::WindowShowState last_show_state);

 private:
  // Update window bounds based on a change in show state.
  void UpdateBoundsFromShowState(aura::Window* window, bool animate);

  // Updates window bounds and animates when requested and possible.
  void MaybeAnimateToBounds(aura::Window* window,
                            bool animate,
                            const gfx::Rect& new_bounds);

  // Adjusts the window sizes when the screen changes its size or its
  // work area insets.
  void AdjustWindowSizesForScreenChange();

  // Set of windows we're listening to.
  WindowSet windows_;

  aura::RootWindow* root_window_;

  DISALLOW_COPY_AND_ASSIGN(BaseLayoutManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_BASE_LAYOUT_MANAGER_H_
