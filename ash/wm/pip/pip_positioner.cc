// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_positioner.h"

#include <algorithm>

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/keyboard/keyboard_controller.h"

namespace ash {

namespace {
const int kPipWorkAreaInsetsDp = 8;

enum { GRAVITY_LEFT, GRAVITY_RIGHT, GRAVITY_TOP, GRAVITY_BOTTOM };

// Returns the result of adjusting |bounds| according to |gravity| inside
// |region|.
gfx::Rect GetAdjustedBoundsByGravity(const gfx::Rect& bounds,
                                     const gfx::Rect& region,
                                     int gravity) {
  switch (gravity) {
    case GRAVITY_LEFT:
      return gfx::Rect(region.x(), bounds.y(), bounds.width(), bounds.height());
    case GRAVITY_RIGHT:
      return gfx::Rect(region.right() - bounds.width(), bounds.y(),
                       bounds.width(), bounds.height());
    case GRAVITY_TOP:
      return gfx::Rect(bounds.x(), region.y(), bounds.width(), bounds.height());
    case GRAVITY_BOTTOM:
      return gfx::Rect(bounds.x(), region.bottom() - bounds.height(),
                       bounds.width(), bounds.height());
    default:
      NOTREACHED();
  }
  return bounds;
}

}  // namespace

gfx::Rect PipPositioner::GetMovementArea(const display::Display& display) {
  gfx::Rect work_area = display.work_area();
  auto* keyboard_controller = keyboard::KeyboardController::Get();

  // Include keyboard if it's not floating.
  if (keyboard_controller->IsEnabled() &&
      keyboard_controller->GetActiveContainerType() !=
          keyboard::ContainerType::FLOATING) {
    gfx::Rect keyboard_bounds = keyboard_controller->visual_bounds_in_screen();
    work_area.Subtract(keyboard_bounds);
  }

  work_area.Inset(kPipWorkAreaInsetsDp, kPipWorkAreaInsetsDp);
  return work_area;
}

gfx::Rect PipPositioner::GetBoundsForDrag(const display::Display& display,
                                          const gfx::Rect& bounds) {
  gfx::Rect drag_bounds = bounds;
  drag_bounds.AdjustToFit(GetMovementArea(display));
  return drag_bounds;
}

gfx::Rect PipPositioner::GetRestingPosition(const display::Display& display,
                                            const gfx::Rect& bounds) {
  gfx::Rect resting_bounds = bounds;
  gfx::Rect area = GetMovementArea(display);
  resting_bounds.AdjustToFit(area);
  gfx::Point pip_center = resting_bounds.CenterPoint();
  gfx::Point area_center = area.CenterPoint();

  gfx::Vector2d direction = pip_center - area_center;
  int gravity = 0;
  if (std::abs(direction.x()) > std::abs(direction.y())) {
    gravity = direction.x() < 0 ? GRAVITY_LEFT : GRAVITY_RIGHT;
  } else {
    gravity = direction.y() < 0 ? GRAVITY_TOP : GRAVITY_BOTTOM;
  }
  return GetAdjustedBoundsByGravity(resting_bounds, area, gravity);
}

}  // namespace ash
