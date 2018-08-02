// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_app_window_drag_controller.h"

#include "ash/shell.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/tablet_mode/tablet_mode_window_drag_delegate.h"
#include "ash/wm/window_state.h"
#include "ui/base/hit_test.h"
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

// The drag delegate for app windows. It not only includes the logic in
// TabletModeWindowDragDelegate, but also has special logic for app windows.
class TabletModeAppWindowDragDelegate : public TabletModeWindowDragDelegate {
 public:
  TabletModeAppWindowDragDelegate() = default;
  ~TabletModeAppWindowDragDelegate() override = default;

 private:
  // TabletModeWindowDragDelegate:
  void PrepareForDraggedWindow(const gfx::Point& location_in_screen) override {
    wm::GetWindowState(dragged_window_)
        ->CreateDragDetails(location_in_screen, HTCLIENT,
                            ::wm::WINDOW_MOVE_SOURCE_TOUCH);
  }
  void UpdateForDraggedWindow(const gfx::Point& location_in_screen) override {}
  void EndingForDraggedWindow(
      wm::WmToplevelWindowEventHandler::DragResult result,
      const gfx::Point& location_in_screen) override {
    wm::GetWindowState(dragged_window_)->DeleteDragDetails();
  }

  DISALLOW_COPY_AND_ASSIGN(TabletModeAppWindowDragDelegate);
};

}  // namespace

TabletModeAppWindowDragController::TabletModeAppWindowDragController()
    : drag_delegate_(std::make_unique<TabletModeAppWindowDragDelegate>()) {}

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
  drag_delegate_->StartWindowDrag(static_cast<aura::Window*>(event->target()),
                                  GetEventLocationInScreen(event));
}

void TabletModeAppWindowDragController::UpdateWindowDrag(
    ui::GestureEvent* event) {
  // Update the dragged window's tranform during dragging.
  drag_delegate_->ContinueWindowDrag(
      GetEventLocationInScreen(event),
      TabletModeWindowDragDelegate::UpdateDraggedWindowType::UPDATE_TRANSFORM);
}

void TabletModeAppWindowDragController::EndWindowDrag(
    ui::GestureEvent* event,
    wm::WmToplevelWindowEventHandler::DragResult result) {
  if (event->type() == ui::ET_SCROLL_FLING_START &&
      event->details().velocity_y() > kFlingToOverviewThreshold) {
    DCHECK(Shell::Get()->window_selector_controller()->IsSelecting());
    Shell::Get()->window_selector_controller()->window_selector()->AddItem(
        drag_delegate_->dragged_window(), /*reposition=*/true,
        /*animate=*/true);
  }
  drag_delegate_->EndWindowDrag(result, GetEventLocationInScreen(event));
}

}  // namespace ash
