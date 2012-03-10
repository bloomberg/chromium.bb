// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_BASE_LAYOUT_MANAGER_H_
#define ASH_WM_BASE_LAYOUT_MANAGER_H_
#pragma once

#include <set>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/root_window_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
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
                                     public aura::WindowObserver {
 public:
  typedef std::set<aura::Window*> WindowSet;

  BaseLayoutManager();
  virtual ~BaseLayoutManager();

  const WindowSet& windows() const { return windows_; }

  // LayoutManager overrides:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  // RootWindowObserver overrides:
  virtual void OnRootWindowResized(const gfx::Size& new_size) OVERRIDE;
  virtual void OnScreenWorkAreaInsetsChanged() OVERRIDE;

  // WindowObserver overrides:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;

 private:
  // Update window bounds based on a change in show state.
  void UpdateBoundsFromShowState(aura::Window* window);

  // Adjusts the window sizes when the screen changes its size or its
  // work area insets.
  void AdjustWindowSizesForScreenChange();

  // Set of windows we're listening to.
  WindowSet windows_;

  DISALLOW_COPY_AND_ASSIGN(BaseLayoutManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_BASE_LAYOUT_MANAGER_H_
