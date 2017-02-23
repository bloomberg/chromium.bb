// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/drag_window_resizer.h"

#include "ash/common/wm/window_positioning_utils.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_window.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/shell.h"
#include "ash/wm/drag_window_controller.h"
#include "ash/wm/window_util.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

namespace ash {

// static
DragWindowResizer* DragWindowResizer::instance_ = NULL;

DragWindowResizer::~DragWindowResizer() {
  if (window_state_)
    window_state_->DeleteDragDetails();
  Shell* shell = Shell::GetInstance();
  shell->mouse_cursor_filter()->set_mouse_warp_enabled(true);
  shell->mouse_cursor_filter()->HideSharedEdgeIndicator();
  if (instance_ == this)
    instance_ = NULL;
}

// static
DragWindowResizer* DragWindowResizer::Create(WindowResizer* next_window_resizer,
                                             wm::WindowState* window_state) {
  return new DragWindowResizer(next_window_resizer, window_state);
}

void DragWindowResizer::Drag(const gfx::Point& location, int event_flags) {
  base::WeakPtr<DragWindowResizer> resizer(weak_ptr_factory_.GetWeakPtr());
  next_window_resizer_->Drag(location, event_flags);

  if (!resizer)
    return;

  last_mouse_location_ = location;
  // Show a phantom window for dragging in another root window.
  if (display::Screen::GetScreen()->GetNumDisplays() > 1) {
    gfx::Point location_in_screen = location;
    ::wm::ConvertPointToScreen(GetAuraTarget()->parent(), &location_in_screen);
    UpdateDragWindow(GetAuraTarget()->bounds(), location_in_screen);
  } else {
    drag_window_controller_.reset();
  }
}

void DragWindowResizer::CompleteDrag() {
  next_window_resizer_->CompleteDrag();

  GetAuraTarget()->layer()->SetOpacity(details().initial_opacity);
  drag_window_controller_.reset();

  // Check if the destination is another display.
  gfx::Point last_mouse_location_in_screen = last_mouse_location_;
  ::wm::ConvertPointToScreen(GetAuraTarget()->parent(),
                             &last_mouse_location_in_screen);
  display::Screen* screen = display::Screen::GetScreen();
  const display::Display dst_display =
      screen->GetDisplayNearestPoint(last_mouse_location_in_screen);

  if (dst_display.id() !=
      screen->GetDisplayNearestWindow(GetAuraTarget()->GetRootWindow()).id()) {
    // Adjust the size and position so that it doesn't exceed the size of
    // work area.
    const gfx::Size& size = dst_display.work_area().size();
    gfx::Rect bounds = GetAuraTarget()->bounds();
    if (bounds.width() > size.width()) {
      int diff = bounds.width() - size.width();
      bounds.set_x(bounds.x() + diff / 2);
      bounds.set_width(size.width());
    }
    if (bounds.height() > size.height())
      bounds.set_height(size.height());

    gfx::Rect dst_bounds = bounds;
    ::wm::ConvertRectToScreen(GetAuraTarget()->parent(), &dst_bounds);

    // Adjust the position so that the cursor is on the window.
    if (!dst_bounds.Contains(last_mouse_location_in_screen)) {
      if (last_mouse_location_in_screen.x() < dst_bounds.x())
        dst_bounds.set_x(last_mouse_location_in_screen.x());
      else if (last_mouse_location_in_screen.x() > dst_bounds.right())
        dst_bounds.set_x(last_mouse_location_in_screen.x() -
                         dst_bounds.width());
    }
    ash::wm::AdjustBoundsToEnsureMinimumWindowVisibility(dst_display.bounds(),
                                                         &dst_bounds);

    GetAuraTarget()->SetBoundsInScreen(dst_bounds, dst_display);
  }
}

void DragWindowResizer::RevertDrag() {
  next_window_resizer_->RevertDrag();

  drag_window_controller_.reset();
  GetAuraTarget()->layer()->SetOpacity(details().initial_opacity);
}

DragWindowResizer::DragWindowResizer(WindowResizer* next_window_resizer,
                                     wm::WindowState* window_state)
    : WindowResizer(window_state),
      next_window_resizer_(next_window_resizer),
      weak_ptr_factory_(this) {
  // The pointer should be confined in one display during resizing a window
  // because the window cannot span two displays at the same time anyway. The
  // exception is window/tab dragging operation. During that operation,
  // |mouse_warp_mode_| should be set to WARP_DRAG so that the user could move a
  // window/tab to another display.
  MouseCursorEventFilter* mouse_cursor_filter =
      Shell::GetInstance()->mouse_cursor_filter();
  mouse_cursor_filter->set_mouse_warp_enabled(ShouldAllowMouseWarp());
  if (ShouldAllowMouseWarp())
    mouse_cursor_filter->ShowSharedEdgeIndicator(
        GetAuraTarget()->GetRootWindow());
  instance_ = this;
}

void DragWindowResizer::UpdateDragWindow(
    const gfx::Rect& bounds_in_parent,
    const gfx::Point& drag_location_in_screen) {
  if (details().window_component != HTCAPTION || !ShouldAllowMouseWarp())
    return;

  if (!drag_window_controller_)
    drag_window_controller_.reset(new DragWindowController(GetAuraTarget()));

  gfx::Rect bounds_in_screen = bounds_in_parent;
  ::wm::ConvertRectToScreen(GetAuraTarget()->parent(), &bounds_in_screen);

  gfx::Rect root_bounds_in_screen =
      GetAuraTarget()->GetRootWindow()->GetBoundsInScreen();
  float opacity = 1.0f;
  if (!root_bounds_in_screen.Contains(drag_location_in_screen)) {
    gfx::Rect visible_bounds = root_bounds_in_screen;
    visible_bounds.Intersect(bounds_in_screen);
    opacity = DragWindowController::GetDragWindowOpacity(bounds_in_screen,
                                                         visible_bounds);
  }
  GetAuraTarget()->layer()->SetOpacity(opacity);
  drag_window_controller_->Update(bounds_in_screen, drag_location_in_screen);
}

bool DragWindowResizer::ShouldAllowMouseWarp() {
  return details().window_component == HTCAPTION &&
         !::wm::GetTransientParent(GetAuraTarget()) &&
         wm::IsWindowUserPositionable(GetAuraTarget());
}

aura::Window* DragWindowResizer::GetAuraTarget() {
  return WmWindow::GetAuraWindow(GetTarget());
}

}  // namespace ash
