// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_browser_window_drag_controller.h"

#include "ash/shell_port.h"
#include "ash/wm/tablet_mode/tablet_mode_browser_window_drag_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

TabletModeBrowserWindowDragController::TabletModeBrowserWindowDragController(
    wm::WindowState* window_state)
    : WindowResizer(window_state),
      drag_delegate_(std::make_unique<TabletModeBrowserWindowDragDelegate>()),
      weak_ptr_factory_(this) {
  DCHECK(details().is_resizable);
  DCHECK(!window_state->allow_set_bounds_direct());

  if (details().source != ::wm::WINDOW_MOVE_SOURCE_TOUCH) {
    ShellPort::Get()->LockCursor();
    did_lock_cursor_ = true;
  }

  previous_location_in_screen_ = details().initial_location_in_parent;
  ::wm::ConvertPointToScreen(GetTarget()->parent(),
                             &previous_location_in_screen_);

  drag_delegate_->StartWindowDrag(GetTarget(), previous_location_in_screen_);
}

TabletModeBrowserWindowDragController::
    ~TabletModeBrowserWindowDragController() {
  if (did_lock_cursor_)
    ShellPort::Get()->UnlockCursor();
}

void TabletModeBrowserWindowDragController::Drag(
    const gfx::Point& location_in_parent,
    int event_flags) {
  // Update dragged window's bounds.
  gfx::Rect bounds = CalculateBoundsForDrag(location_in_parent);
  if (bounds != GetTarget()->bounds()) {
    base::WeakPtr<TabletModeBrowserWindowDragController> resizer(
        weak_ptr_factory_.GetWeakPtr());
    GetTarget()->SetBounds(bounds);
    if (!resizer)
      return;
  }

  gfx::Point location_in_screen = location_in_parent;
  ::wm::ConvertPointToScreen(GetTarget()->parent(), &location_in_screen);
  previous_location_in_screen_ = location_in_screen;

  // Update the drag indicators, snap preview window, source window position,
  // blurred backgournd, etc, if necessary.
  drag_delegate_->ContinueWindowDrag(location_in_screen);
}

void TabletModeBrowserWindowDragController::CompleteDrag() {
  drag_delegate_->EndWindowDrag(
      wm::WmToplevelWindowEventHandler::DragResult::SUCCESS,
      previous_location_in_screen_);
}

void TabletModeBrowserWindowDragController::RevertDrag() {
  drag_delegate_->EndWindowDrag(
      wm::WmToplevelWindowEventHandler::DragResult::REVERT,
      previous_location_in_screen_);
}

}  // namespace ash
