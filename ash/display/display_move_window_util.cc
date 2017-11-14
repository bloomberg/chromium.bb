// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_move_window_util.h"

#include <stdint.h>
#include <array>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace ash {

namespace {

// Calculate the vertical offset between two displays' center points.
int GetVerticalOffset(const display::Display& display1,
                      const display::Display& display2) {
  DCHECK(display1.is_valid());
  DCHECK(display2.is_valid());
  return std::abs(display1.bounds().CenterPoint().y() -
                  display2.bounds().CenterPoint().y());
}

// Calculate the horizontal offset between two displays' center points.
int GetHorizontalOffset(const display::Display& display1,
                        const display::Display& display2) {
  DCHECK(display1.is_valid());
  DCHECK(display2.is_valid());
  return std::abs(display1.bounds().CenterPoint().x() -
                  display2.bounds().CenterPoint().x());
}

// Returns true if |candidate_display| is not completely off the bounds of
// |original_display| in the moving direction or cycled direction.
bool IsWithinBounds(const display::Display& candidate_display,
                    const display::Display& origin_display,
                    DisplayMoveWindowDirection direction) {
  const gfx::Rect& candidate_bounds = candidate_display.bounds();
  const gfx::Rect& origin_bounds = origin_display.bounds();
  if (direction == DisplayMoveWindowDirection::kLeft ||
      direction == DisplayMoveWindowDirection::kRight) {
    return GetHorizontalOffset(candidate_display, origin_display) <
           (candidate_bounds.width() + origin_bounds.width()) / 2;
  }

  return GetVerticalOffset(candidate_display, origin_display) <
         (candidate_bounds.height() + origin_bounds.height()) / 2;
}

// Gets the space between displays. The calculation is based on |direction|.
// Non-negative value indicates |kInMovementDirection| and negative value
// indicates |kInCycledDirection|.
int GetSpaceBetweenDisplays(const display::Display& candidate_display,
                            const display::Display& origin_display,
                            DisplayMoveWindowDirection direction) {
  switch (direction) {
    case DisplayMoveWindowDirection::kAbove:
      return origin_display.bounds().y() - candidate_display.bounds().bottom();
    case DisplayMoveWindowDirection::kBelow:
      return candidate_display.bounds().y() - origin_display.bounds().bottom();
    case DisplayMoveWindowDirection::kLeft:
      return origin_display.bounds().x() - candidate_display.bounds().right();
    case DisplayMoveWindowDirection::kRight:
      return candidate_display.bounds().x() - origin_display.bounds().right();
  }
  NOTREACHED();
  return 0;
}

// Gets the orthogonal offset between displays, which is vertical offset for
// left/right, horizontal offset for above/below.
int GetOrthogonalOffsetBetweenDisplays(
    const display::Display& candidate_display,
    const display::Display& origin_display,
    DisplayMoveWindowDirection direction) {
  if (direction == DisplayMoveWindowDirection::kLeft ||
      direction == DisplayMoveWindowDirection::kRight) {
    return GetVerticalOffset(candidate_display, origin_display);
  }
  return GetHorizontalOffset(candidate_display, origin_display);
}

// Gets the display id of next display of |origin_display| on the moving
// |direction|. Cycled direction result is returned if the moving |direction|
// doesn't contain eligible candidate displays.
int64_t GetNextDisplay(const display::Display& origin_display,
                       DisplayMoveWindowDirection direction) {
  // Saves the next display found in the specified movement direction.
  display::Display next_movement_display;
  // Saves the next display found in the cycled movement direction.
  display::Display next_cycled_display;
  for (const auto& candidate_display :
       display::Screen::GetScreen()->GetAllDisplays()) {
    if (candidate_display.id() == origin_display.id())
      continue;
    // Ignore the candidate display if |IsWithinBounds| returns true. This
    // ensures that |candidate_space| calculated below represents the real
    // layout apart distance.
    if (IsWithinBounds(candidate_display, origin_display, direction))
      continue;
    const int candidate_space =
        GetSpaceBetweenDisplays(candidate_display, origin_display, direction);
    display::Display& next_display =
        candidate_space >= 0 ? next_movement_display : next_cycled_display;

    if (!next_display.is_valid()) {
      next_display = candidate_display;
      continue;
    }

    const int current_space =
        GetSpaceBetweenDisplays(next_display, origin_display, direction);
    if (candidate_space < current_space) {
      next_display = candidate_display;
    } else if (candidate_space == current_space) {
      const int candidate_orthogonal_offset =
          GetOrthogonalOffsetBetweenDisplays(candidate_display, origin_display,
                                             direction);
      const int current_orthogonal_offset = GetOrthogonalOffsetBetweenDisplays(
          next_display, origin_display, direction);
      if (candidate_orthogonal_offset < current_orthogonal_offset)
        next_display = candidate_display;
    }
  }

  return next_movement_display.is_valid() ? next_movement_display.id()
                                          : next_cycled_display.id();
}

}  // namespace

void HandleMoveWindowToDisplay(DisplayMoveWindowDirection direction) {
  aura::Window* window = wm::GetActiveWindow();
  if (!window)
    return;

  display::Display origin_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  int64_t dest_display_id = GetNextDisplay(origin_display, direction);
  if (dest_display_id == display::kInvalidDisplayId)
    return;
  wm::MoveWindowToDisplay(window, dest_display_id);

  // Send a11y alert.
  mojom::AccessibilityAlert alert = mojom::AccessibilityAlert::NONE;
  if (direction == DisplayMoveWindowDirection::kAbove) {
    alert = mojom::AccessibilityAlert::WINDOW_MOVED_TO_ABOVE_DISPLAY;
  } else if (direction == DisplayMoveWindowDirection::kBelow) {
    alert = mojom::AccessibilityAlert::WINDOW_MOVED_TO_BELOW_DISPLAY;
  } else if (direction == DisplayMoveWindowDirection::kLeft) {
    alert = mojom::AccessibilityAlert::WINDOW_MOVED_TO_LEFT_DISPLAY;
  } else {
    DCHECK(direction == DisplayMoveWindowDirection::kRight);
    alert = mojom::AccessibilityAlert::WINDOW_MOVED_TO_RIGHT_DISPLAY;
  }
  Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(alert);
}

}  // namespace ash
