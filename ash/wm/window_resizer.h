// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESIZER_H_
#define ASH_WM_WINDOW_RESIZER_H_
#pragma once

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}

namespace ash {

namespace internal {
class RootWindowEventFilter;
}

// WindowResizer is used by ToplevelWindowEventFilter to handle dragging,
// moving or resizing a window.
class ASH_EXPORT WindowResizer {
 public:
  // Constants to identify the type of resize.
  static const int kBoundsChange_None;
  static const int kBoundsChange_Repositions;
  static const int kBoundsChange_Resizes;

  // Used to indicate which direction the resize occurs in.
  static const int kBoundsChangeDirection_None;
  static const int kBoundsChangeDirection_Horizontal;
  static const int kBoundsChangeDirection_Vertical;

  WindowResizer(aura::Window* window,
                const gfx::Point& location,
                int window_component,
                int grid_size);
  virtual ~WindowResizer();

  // Returns a bitmask of the kBoundsChange_ values.
  static int GetBoundsChangeForWindowComponent(int component);

  // Returns a location >= |location| that is aligned to fall on increments of
  // |grid_size|.
  static int AlignToGrid(int location, int grid_size);

  // Invoked to drag/move/resize the window. |location| is in the coordinates
  // of the window supplied to the constructor.
  void Drag(const gfx::Point& location);

  // Invoked to complete the drag.
  virtual void CompleteDrag();

  // Reverts the drag.
  virtual void RevertDrag();

  // Returns true if the drag will result in changing the window in anyway.
  bool is_resizable() const { return is_resizable_; }

  // See description above members for details.
  const gfx::Rect& initial_bounds() const { return initial_bounds_; }
  const gfx::Point& initial_location_in_parent() const {
    return initial_location_in_parent_;
  }
  int window_component() const { return window_component_; }
  aura::Window* window() const { return window_; }
  int grid_size() const { return grid_size_; }
  bool did_move_or_resize() const { return did_move_or_resize_; }
  int bounds_change() const { return bounds_change_; }

 protected:
  // Returns the bounds to give to the window once the mouse has moved to
  // |location|.
  virtual gfx::Rect GetBoundsForDrag(const gfx::Point& location);

  // Returns the final bounds. This differs from current bounds if a grid_size
  // was specified.
  virtual gfx::Rect GetFinalBounds();

 private:
  // Returns the new origin of the window. The arguments are the difference
  // between the current location and the initial location.
  gfx::Point GetOriginForDrag(int delta_x, int delta_y) const;

  // Returns the size of the window for the drag.
  gfx::Size GetSizeForDrag(int* delta_x, int* delta_y) const;

  // Returns the width of the window.
  int GetWidthForDrag(int min_width, int* delta_x) const;

  // Returns the height of the drag.
  int GetHeightForDrag(int min_height, int* delta_y) const;

  // The window we're resizing.
  aura::Window* window_;

  // Initial bounds of the window.
  const gfx::Rect initial_bounds_;

  // Location passed to the constructor, in |window->parent()|'s coordinates.
  const gfx::Point initial_location_in_parent_;

  // The component the user pressed on.
  const int window_component_;

  // Bitmask of the |kBoundsChange_| constants.
  const int bounds_change_;

  // Bitmask of the |kBoundsChangeDirection_| constants.
  const int position_change_direction_;

  // Bitmask of the |kBoundsChangeDirection_| constants.
  const int size_change_direction_;

  // Will the drag actually modify the window?
  const bool is_resizable_;

  // Size of the grid.
  const int grid_size_;

  // Set to true once Drag() is invoked and the bounds of the window change.
  bool did_move_or_resize_;

  internal::RootWindowEventFilter* root_filter_;

  DISALLOW_COPY_AND_ASSIGN(WindowResizer);
};

}  // namespace aura

#endif  // ASH_WM_WINDOW_RESIZER_H_
