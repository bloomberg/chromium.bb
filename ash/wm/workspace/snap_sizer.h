// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_SNAP_SIZER_H_
#define ASH_WM_WORKSPACE_SNAP_SIZER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/time.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}

namespace ash {
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

  SnapSizer(aura::Window* window, const gfx::Point& start, Edge edge);

  // Updates the target bounds based on a mouse move.
  void Update(const gfx::Point& location);

  // Bounds to position the window at.
  const gfx::Rect& target_bounds() const { return target_bounds_; }

  // Returns the appropriate snap bounds (e.g. if a window is already snapped,
  // then it returns the next snap-bounds).
  gfx::Rect GetSnapBounds(const gfx::Rect& bounds);

  // Set the snap sizer to the button press default size and prevent resizing.
  void SelectDefaultSizeAndDisableResize();

  // Returns the target bounds based on the edge and the provided |size_index|.
  // For unit test purposes this function is not private.
  gfx::Rect GetTargetBoundsForSize(size_t size_index) const;

 private:
  // Calculates the amount to increment by. This returns one of -1, 0 or 1 and
  // is intended to by applied to |size_index_|. |x| is the current
  // x-coordinate, and |reference_x| is used to determine whether to increase
  // or decrease the position. It's one of |last_adjust_x_| or |last_update_x_|.
  int CalculateIncrement(int x, int reference_x) const;

  // Changes the bounds. |x| is the current x-coordinate and |delta| the amount
  // to increase by. |delta| comes from CalculateIncrement() and is applied
  // to |size_index_|.
  void ChangeBounds(int x, int delta);

  // Returns the target bounds based on the edge and |size_index_|.
  gfx::Rect GetTargetBounds() const;

  // Returns true if the specified point is along the edge of the screen.
  bool AlongEdge(int x) const;

  // Window being snapped.
  aura::Window* window_;

  const Edge edge_;

  // Current target bounds for the snap.
  gfx::Rect target_bounds_;

  // Time Update() was last invoked.
  base::TimeTicks time_last_update_;

  // Index into |kSizes| that dictates the width of the screen the target
  // bounds should get.
  int size_index_;

  // If set, |size_index_| will get ignored and the single button default
  // setting will be used instead.
  bool resize_disabled_;

  // Number of times Update() has been invoked since last ChangeBounds().
  int num_moves_since_adjust_;

  // X-coordinate the last time ChangeBounds() was invoked.
  int last_adjust_x_;

  // X-coordinate last supplied to Update().
  int last_update_x_;

  DISALLOW_COPY_AND_ASSIGN(SnapSizer);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_SNAP_SIZER_H_
