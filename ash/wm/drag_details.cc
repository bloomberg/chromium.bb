// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/drag_details.h"

#include "ash/wm/window_resizer.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"

namespace ash {

namespace {

int GetSizeChangeDirectionForWindowComponent(int window_component) {
  int size_change_direction = WindowResizer::kBoundsChangeDirection_None;
  switch (window_component) {
    case HTTOPLEFT:
    case HTTOPRIGHT:
    case HTBOTTOMLEFT:
    case HTBOTTOMRIGHT:
    case HTGROWBOX:
    case HTCAPTION:
      size_change_direction |=
          WindowResizer::kBoundsChangeDirection_Horizontal |
          WindowResizer::kBoundsChangeDirection_Vertical;
      break;
    case HTTOP:
    case HTBOTTOM:
      size_change_direction |= WindowResizer::kBoundsChangeDirection_Vertical;
      break;
    case HTRIGHT:
    case HTLEFT:
      size_change_direction |= WindowResizer::kBoundsChangeDirection_Horizontal;
      break;
    default:
      break;
  }
  return size_change_direction;
}

}  // namespace

DragDetails::DragDetails(aura::Window* window,
                         const gfx::Point& location,
                         int window_component,
                         aura::client::WindowMoveSource source)
    : initial_state_type(wm::GetWindowState(window)->GetStateType()),
      initial_bounds_in_parent(window->bounds()),
      initial_location_in_parent(location),
      initial_opacity(window->layer()->opacity()),
      window_component(window_component),
      bounds_change(
          WindowResizer::GetBoundsChangeForWindowComponent(window_component)),
      position_change_direction(
          WindowResizer::GetPositionChangeDirectionForWindowComponent(
              window_component)),
      size_change_direction(
          GetSizeChangeDirectionForWindowComponent(window_component)),
      is_resizable(bounds_change != WindowResizer::kBoundsChangeDirection_None),
      source(source),
      should_attach_to_shelf(window->type() == ui::wm::WINDOW_TYPE_PANEL &&
                             wm::GetWindowState(window)->panel_attached()) {
  wm::WindowState* window_state = wm::GetWindowState(window);
  if (window_state->IsNormalOrSnapped() &&
      window_state->HasRestoreBounds() &&
      window_component == HTCAPTION) {
    restore_bounds = window_state->GetRestoreBoundsInScreen();
  }
}

DragDetails::~DragDetails() {
}

}  // namespace ash
