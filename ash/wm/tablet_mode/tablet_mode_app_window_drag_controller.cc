// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_app_window_drag_controller.h"

#include "ash/shell.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/tablet_mode/tablet_mode_window_drag_delegate.h"
#include "ash/wm/window_state.h"
#include "ui/base/hit_test.h"
#include "ui/display/screen.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// Return the location of |event| in screen coordinates.
gfx::Point GetEventLocationInScreen(const ui::GestureEvent* event) {
  gfx::Point location_in_screen(event->location());
  ::wm::ConvertPointToScreen(static_cast<aura::Window*>(event->target()),
                             &location_in_screen);
  return location_in_screen;
}

}  // namespace

TabletModeAppWindowDragController::TabletModeAppWindowDragController()
    : drag_delegate_(std::make_unique<TabletModeWindowDragDelegate>()) {}

TabletModeAppWindowDragController::~TabletModeAppWindowDragController() =
    default;

bool TabletModeAppWindowDragController::DragWindowFromTop(
    ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    StartWindowDrag(event);
    return true;
  }

  if (!drag_delegate_->dragged_window())
    return false;

  if (event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    UpdateWindowDrag(event);
    return true;
  }

  if (event->type() == ui::ET_GESTURE_SCROLL_END ||
      event->type() == ui::ET_SCROLL_FLING_START) {
    EndWindowDrag(event, wm::WmToplevelWindowEventHandler::DragResult::SUCCESS);
    return true;
  }

  EndWindowDrag(event, wm::WmToplevelWindowEventHandler::DragResult::REVERT);
  return false;
}

void TabletModeAppWindowDragController::StartWindowDrag(
    ui::GestureEvent* event) {
  drag_delegate_->OnWindowDragStarted(
      static_cast<aura::Window*>(event->target()));
  initial_location_in_screen_ = GetEventLocationInScreen(event);

  wm::GetWindowState(drag_delegate_->dragged_window())
      ->CreateDragDetails(initial_location_in_screen_, HTCLIENT,
                          ::wm::WINDOW_MOVE_SOURCE_TOUCH);
  if (!Shell::Get()->window_selector_controller()->IsSelecting())
    Shell::Get()->window_selector_controller()->ToggleOverview();

  gesture_drag_amount_.SetPoint(0.f, 0.f);
}

void TabletModeAppWindowDragController::UpdateWindowDrag(
    ui::GestureEvent* event) {
  const gfx::Point location_in_screen(GetEventLocationInScreen(event));
  drag_delegate_->UpdateIndicatorState(location_in_screen);

  gesture_drag_amount_ +=
      gfx::Vector2dF(event->details().scroll_x(), event->details().scroll_y());
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(drag_delegate_->dragged_window())
          .bounds();
  const float x_scale =
      1.0f - fabsf(gesture_drag_amount_.x()) / display_bounds.width();
  const float y_scale =
      1.0f - fabsf(gesture_drag_amount_.y()) / display_bounds.height();
  const float scale = std::min(x_scale, y_scale);
  gfx::Transform transform;
  transform.Translate(
      location_in_screen.x() - initial_location_in_screen_.x() * scale,
      location_in_screen.y() - initial_location_in_screen_.y() * scale);
  transform.Scale(scale, scale);
  drag_delegate_->dragged_window()->SetTransform(transform);
}

void TabletModeAppWindowDragController::EndWindowDrag(
    ui::GestureEvent* event,
    wm::WmToplevelWindowEventHandler::DragResult result) {
  const gfx::Point location_in_screen(GetEventLocationInScreen(event));
  drag_delegate_->dragged_window()->SetTransform(gfx::Transform());
  wm::GetWindowState(drag_delegate_->dragged_window())->DeleteDragDetails();
  drag_delegate_->OnWindowDragEnded(result, location_in_screen);
  gesture_drag_amount_.SetPoint(0.f, 0.f);
}

}  // namespace ash
