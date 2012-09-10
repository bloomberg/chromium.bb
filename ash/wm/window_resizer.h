// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESIZER_H_
#define ASH_WM_WINDOW_RESIZER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}

namespace ash {

// WindowResizer is used by ToplevelWindowEventFilter to handle dragging, moving
// or resizing a window. All coordinates passed to this are in the parent
// windows coordinates.
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

  WindowResizer();
  virtual ~WindowResizer();

  // Returns a bitmask of the kBoundsChange_ values.
  static int GetBoundsChangeForWindowComponent(int component);

  // Invoked to drag/move/resize the window. |location| is in the coordinates
  // of the window supplied to the constructor. |event_flags| is the event
  // flags from the event.
  virtual void Drag(const gfx::Point& location, int event_flags) = 0;

  // Invoked to complete the drag.
  virtual void CompleteDrag(int event_flags) = 0;

  // Reverts the drag.
  virtual void RevertDrag() = 0;

  // Returns the target window the resizer was created for.
  virtual aura::Window* GetTarget() = 0;

 protected:
  struct Details {
    Details();
    Details(aura::Window* window,
            const gfx::Point& location,
            int window_component);
    ~Details();

    // The window we're resizing.
    aura::Window* window;

    // Initial bounds of the window.
    gfx::Rect initial_bounds;

    // Restore bounds (in screen coordinates) of the window before the drag
    // started. Only set if the window is normal and is being dragged.
    gfx::Rect restore_bounds;

    // Location passed to the constructor, in |window->parent()|'s coordinates.
    gfx::Point initial_location_in_parent;

    // Initial opacity of the window.
    float initial_opacity;

    // The component the user pressed on.
    int window_component;

    // Bitmask of the |kBoundsChange_| constants.
    int bounds_change;

    // Bitmask of the |kBoundsChangeDirection_| constants.
    int position_change_direction;

    // Bitmask of the |kBoundsChangeDirection_| constants.
    int size_change_direction;

    // Will the drag actually modify the window?
    bool is_resizable;
  };

  static gfx::Rect CalculateBoundsForDrag(const Details& details,
                                          const gfx::Point& location);

  static gfx::Rect AdjustBoundsToGrid(const gfx::Rect& bounds,
                                      int grid_size);

  static bool IsBottomEdge(int component);

 private:
  // Returns the new origin of the window. The arguments are the difference
  // between the current location and the initial location.
  static gfx::Point GetOriginForDrag(const Details& details,
                                     int delta_x,
                                     int delta_y);

  // Returns the size of the window for the drag.
  static gfx::Size GetSizeForDrag(const Details& details,
                                  int* delta_x,
                                  int* delta_y);

  // Returns the width of the window.
  static int GetWidthForDrag(const Details& details,
                             int min_width,
                             int* delta_x);

  // Returns the height of the drag.
  static int GetHeightForDrag(const Details& details,
                              int min_height,
                              int* delta_y);
};

}  // namespace aura

#endif  // ASH_WM_WINDOW_RESIZER_H_
