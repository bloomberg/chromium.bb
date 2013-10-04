// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_WINDOW_SELECTOR_ITEM_H_
#define ASH_WM_OVERVIEW_WINDOW_SELECTOR_ITEM_H_

#include "base/compiler_specific.h"
#include "ui/gfx/rect.h"

namespace aura {
class RootWindow;
class Window;
}

namespace ash {

// This class represents an item in overview mode. An item can have one or more
// windows, of which only one can be activated by keyboard (i.e. alt+tab) but
// any can be selected with a pointer (touch or mouse).
class WindowSelectorItem {
 public:
  WindowSelectorItem();
  virtual ~WindowSelectorItem();

  // Returns the root window on which this item is shown.
  virtual aura::RootWindow* GetRootWindow() = 0;

  // Returns the targeted window given the event |target| window.
  // Returns NULL if no Window in this item was selected.
  virtual aura::Window* TargetedWindow(const aura::Window* target) = 0;

  // Restores |window| on exiting window overview rather than returning it
  // to its previous state.
  virtual void RestoreWindowOnExit(aura::Window* window) = 0;

  // Returns the |window| to activate on selecting of this item.
  virtual aura::Window* SelectionWindow() = 0;

  // Removes |window| from this item. Check empty() after calling this to see
  // if the entire item is now empty.
  virtual void RemoveWindow(const aura::Window* window) = 0;

  // Returns true if this item has no more selectable windows (i.e. after
  // calling RemoveWindow for the last contained window).
  virtual bool empty() const = 0;

  // Dispatched before beginning window overview. This will do any necessary
  // one time actions such as restoring minimized windows.
  virtual void PrepareForOverview() = 0;

  // Sets the bounds of this window selector item to |target_bounds| in the
  // |root_window| root window.
  void SetBounds(aura::RootWindow* root_window,
                 const gfx::Rect& target_bounds);

  // Recomputes the positions for the windows in this selection item. This is
  // dispatched when the bounds of a window change.
  void RecomputeWindowTransforms();

  // Returns the current bounds of this selector item.
  const gfx::Rect& bounds() { return bounds_; }

 protected:
  // Sets the bounds of this selector item to |target_bounds| in |root_window|.
  // If |animate| the windows are animated from their current location.
  virtual void SetItemBounds(aura::RootWindow* root_window,
                             const gfx::Rect& target_bounds,
                             bool animate) = 0;

 private:
  // The root window this item is being displayed on.
  aura::RootWindow* root_window_;

  // The bounds this item is fit to.
  gfx::Rect bounds_;

  // True if running SetItemBounds. This prevents recursive calls resulting from
  // the bounds update when calling views::corewm::RecreateWindowLayers to copy
  // a window layer for display on another monitor.
  bool in_bounds_update_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorItem);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WINDOW_SELECTOR_ITEM_H_
