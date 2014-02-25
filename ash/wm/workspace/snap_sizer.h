// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_SNAP_SIZER_H_
#define ASH_WM_WORKSPACE_SNAP_SIZER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/time/time.h"
#include "ui/gfx/rect.h"

namespace ash {
namespace wm {
class WindowState;
}

namespace internal {

// SnapSizer is responsible for determining the resulting bounds of a window
// that is being snapped to the left or right side of the screen.
// The bounds used in this class are in the container's coordinates.
class ASH_EXPORT SnapSizer {
 public:
  enum Edge {
    LEFT_EDGE,
    RIGHT_EDGE
  };

  enum InputType {
    TOUCH_DRAG_INPUT,
    OTHER_INPUT
  };

  // Snapping is made easier with a touch drag when using touchscreen.
  static const int kScreenEdgeInsetForTouchDrag;

  // Set |input_type| to |TOUCH_MAXIMIZE_BUTTON_INPUT| when called by a touch
  // operation by the maximize button. This will allow the user to snap resize
  // the window beginning close to the border.
  SnapSizer(wm::WindowState* window_state,
            const gfx::Point& start,
            Edge edge,
            InputType input_type);
  virtual ~SnapSizer();

  // Snaps a window left or right.
  static void SnapWindow(wm::WindowState* window_state, Edge edge);

  // Snaps |window_| to the target bounds.
  void SnapWindowToTargetBounds();

  // Updates the target bounds based on a mouse move.
  void Update(const gfx::Point& location);

  // Returns bounds to position the window at.
  const gfx::Rect& target_bounds() const { return target_bounds_; }

  // Returns true when snapping sequence is at its end (docking).
  bool end_of_sequence() const { return end_of_sequence_; }

 private:
  // Returns the target bounds based on the edge.
  gfx::Rect CalculateTargetBounds() const;

  // Check if we can advance [to docked state]. |x| is the current x-coordinate
  // and |reference_x| is one of |last_adjust_x_| or |last_update_x_|. It is
  // used to determine whether the position continues to change in the original
  // direction. If we advance in the same direction then the snapping sequence
  // reaches its end (and additional drag may cause docking).
  void CheckEndOfSequence(int x, int reference_x);

  // Returns true if the specified point is along the edge of the screen.
  bool AlongEdge(int x) const;

  // WindowState of the window being snapped.
  wm::WindowState* window_state_;

  const Edge edge_;

  // Current target bounds for the snap.
  gfx::Rect target_bounds_;

  // Time Update() was last invoked.
  base::TimeTicks time_last_update_;

  // Set to true after a sufficient drag in the same direction.
  bool end_of_sequence_;

  // Number of times Update() has been invoked since last ChangeBounds().
  int num_moves_since_adjust_;

  // X-coordinate the last time ChangeBounds() was invoked.
  int last_adjust_x_;

  // X-coordinate last supplied to Update().
  int last_update_x_;

  // Initial x-coordinate.
  const int start_x_;

  // |TOUCH_MAXIMIZE_BUTTON_INPUT| if the snap sizer was created through a
  // touch & drag operation of the maximizer button. It changes the behavior of
  // the drag / resize behavior when the dragging starts close to the border.
  const InputType input_type_;

  DISALLOW_COPY_AND_ASSIGN(SnapSizer);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_SNAP_SIZER_H_
