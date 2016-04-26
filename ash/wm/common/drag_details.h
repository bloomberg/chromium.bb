// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_DRAG_DETAILS_H_
#define ASH_WM_COMMON_DRAG_DETAILS_H_

#include "ash/ash_export.h"
#include "ash/wm/common/wm_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/public/window_move_client.h"

namespace aura {
class Window;
}

namespace ash {

namespace wm {
class WmWindow;
class WindowState;
}

struct ASH_EXPORT DragDetails {
  DragDetails(wm::WmWindow* window,
              const gfx::Point& location,
              int window_component,
              // TODO(sky): make wm type.
              aura::client::WindowMoveSource source);
  ~DragDetails();

  ash::wm::WindowStateType initial_state_type;

  // Initial bounds of the window in parent coordinates.
  const gfx::Rect initial_bounds_in_parent;

  // Restore bounds (in screen coordinates) of the window before the drag
  // started. Only set if the window is normal and is being dragged.
  gfx::Rect restore_bounds;

  // Location passed to the constructor, in |window->parent()|'s coordinates.
  const gfx::Point initial_location_in_parent;

  // Initial opacity of the window.
  const float initial_opacity;

  // The component the user pressed on.
  const int window_component;

  // Bitmask of the |kBoundsChange_| constants.
  const int bounds_change;

  // Bitmask of the |kBoundsChangeDirection_| constants.
  const int position_change_direction;

  // Bitmask of the |kBoundsChangeDirection_| constants.
  const int size_change_direction;

  // Will the drag actually modify the window?
  const bool is_resizable;

  // Source of the event initiating the drag.
  const aura::client::WindowMoveSource source;

  // True if the window should attach to the shelf after releasing.
  bool should_attach_to_shelf;
};

}  // namespace ash

#endif  // ASH_WM_COMMON_DRAG_DETAILS_H_
