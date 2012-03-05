// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WINDOW_RESIZER_H_
#define ASH_WM_WORKSPACE_WINDOW_RESIZER_H_
#pragma once

#include "ash/wm/window_resizer.h"
#include "base/compiler_specific.h"

namespace ash {
namespace internal {

class RootWindowEventFilter;

// WindowResizer implementation for workspaces. This enforces that windows are
// not allowed to vertically move or resize outside of the work area. As windows
// are moved outside the work area they are shrunk. We remember the height of
// the window before it was moved so that if the window is again moved up we
// attempt to restore the old height.
class ASH_EXPORT WorkspaceWindowResizer : public WindowResizer {
 public:
  virtual ~WorkspaceWindowResizer();

  // Creates a new WorkspaceWindowResizer. The caller takes ownership of the
  // returned object. Returns NULL if not resizable.
  static WorkspaceWindowResizer* Create(
      aura::Window* window,
      const gfx::Point& location,
      int window_component,
      int grid_size);

  // Returns true if the drag will result in changing the window in anyway.
  bool is_resizable() const { return details_.is_resizable; }

  const gfx::Point& initial_location_in_parent() const {
    return details_.initial_location_in_parent;
  }

  // Overridden from WindowResizer:
  virtual void Drag(const gfx::Point& location) OVERRIDE;
  virtual void CompleteDrag() OVERRIDE;
  virtual void RevertDrag() OVERRIDE;

 private:
  explicit WorkspaceWindowResizer(const Details& details);

  // Adjusts the bounds to enforce that windows are vertically contained in the
  // work area.
  void AdjustBoundsForMainWindow(gfx::Rect* bounds) const;
  void AdjustBoundsForWindow(const gfx::Rect& work_area,
                             aura::Window* window,
                             gfx::Rect* bounds) const;

  // Clears the cached height of the window being dragged.
  void ClearCachedHeights();

  // Returns true if the window touches the bottom of the work area.
  bool TouchesBottomOfScreen() const;

  aura::Window* window() const { return details_.window; }

  const Details details_;

  // True if the window size (height) should be constrained.
  const bool constrain_size_;

  // Set to true once Drag() is invoked and the bounds of the window change.
  bool did_move_or_resize_;

  internal::RootWindowEventFilter* root_filter_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceWindowResizer);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WINDOW_RESIZER_H_
