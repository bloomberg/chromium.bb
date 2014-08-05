// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/drag_window_resizer.h"

#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/drag_window_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/screen.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// The maximum opacity of the drag phantom window.
const float kMaxOpacity = 0.8f;

// Returns true if Ash has more than one root window.
bool HasSecondaryRootWindow() {
  return Shell::GetAllRootWindows().size() > 1;
}

// When there are two root windows, returns one of the root windows which is not
// |root_window|. Returns NULL if only one root window exists.
aura::Window* GetAnotherRootWindow(aura::Window* root_window) {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  if (root_windows.size() < 2)
    return NULL;
  DCHECK_EQ(2U, root_windows.size());
  if (root_windows[0] == root_window)
    return root_windows[1];
  return root_windows[0];
}

}  // namespace

// static
DragWindowResizer* DragWindowResizer::instance_ = NULL;

DragWindowResizer::~DragWindowResizer() {
  if (window_state_)
    window_state_->DeleteDragDetails();
  Shell* shell = Shell::GetInstance();
  shell->mouse_cursor_filter()->set_mouse_warp_mode(
      MouseCursorEventFilter::WARP_ALWAYS);
  shell->mouse_cursor_filter()->HideSharedEdgeIndicator();
  if (instance_ == this)
    instance_ = NULL;
}

// static
DragWindowResizer* DragWindowResizer::Create(
    WindowResizer* next_window_resizer,
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
  if (HasSecondaryRootWindow()) {
    gfx::Point location_in_screen = location;
    wm::ConvertPointToScreen(GetTarget()->parent(), &location_in_screen);
    const bool in_original_root =
        wm::GetRootWindowAt(location_in_screen) == GetTarget()->GetRootWindow();
    UpdateDragWindow(GetTarget()->bounds(), in_original_root);
  } else {
    drag_window_controller_.reset();
  }
}

void DragWindowResizer::CompleteDrag() {
  next_window_resizer_->CompleteDrag();

  GetTarget()->layer()->SetOpacity(details().initial_opacity);
  drag_window_controller_.reset();

  // Check if the destination is another display.
  gfx::Point last_mouse_location_in_screen = last_mouse_location_;
  wm::ConvertPointToScreen(GetTarget()->parent(),
                           &last_mouse_location_in_screen);
  gfx::Screen* screen = Shell::GetScreen();
  const gfx::Display dst_display =
      screen->GetDisplayNearestPoint(last_mouse_location_in_screen);

  if (dst_display.id() !=
      screen->GetDisplayNearestWindow(GetTarget()->GetRootWindow()).id()) {
    // Adjust the size and position so that it doesn't exceed the size of
    // work area.
    const gfx::Size& size = dst_display.work_area().size();
    gfx::Rect bounds = GetTarget()->bounds();
    if (bounds.width() > size.width()) {
      int diff = bounds.width() - size.width();
      bounds.set_x(bounds.x() + diff / 2);
      bounds.set_width(size.width());
    }
    if (bounds.height() > size.height())
      bounds.set_height(size.height());

    gfx::Rect dst_bounds =
        ScreenUtil::ConvertRectToScreen(GetTarget()->parent(), bounds);

    // Adjust the position so that the cursor is on the window.
    if (!dst_bounds.Contains(last_mouse_location_in_screen)) {
      if (last_mouse_location_in_screen.x() < dst_bounds.x())
        dst_bounds.set_x(last_mouse_location_in_screen.x());
      else if (last_mouse_location_in_screen.x() > dst_bounds.right())
        dst_bounds.set_x(
            last_mouse_location_in_screen.x() - dst_bounds.width());
    }
    ash::wm::AdjustBoundsToEnsureMinimumWindowVisibility(
        dst_display.bounds(), &dst_bounds);

    GetTarget()->SetBoundsInScreen(dst_bounds, dst_display);
  }
}

void DragWindowResizer::RevertDrag() {
  next_window_resizer_->RevertDrag();

  drag_window_controller_.reset();
  GetTarget()->layer()->SetOpacity(details().initial_opacity);
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
  mouse_cursor_filter->set_mouse_warp_mode(
      ShouldAllowMouseWarp() ?
      MouseCursorEventFilter::WARP_DRAG : MouseCursorEventFilter::WARP_NONE);
  if (ShouldAllowMouseWarp())
    mouse_cursor_filter->ShowSharedEdgeIndicator(GetTarget()->GetRootWindow());
  instance_ = this;
}

void DragWindowResizer::UpdateDragWindow(const gfx::Rect& bounds,
                                         bool in_original_root) {
  if (details().window_component != HTCAPTION || !ShouldAllowMouseWarp())
    return;

  // It's available. Show a phantom window on the display if needed.
  aura::Window* another_root =
      GetAnotherRootWindow(GetTarget()->GetRootWindow());
  const gfx::Rect root_bounds_in_screen(another_root->GetBoundsInScreen());
  const gfx::Rect bounds_in_screen =
      ScreenUtil::ConvertRectToScreen(GetTarget()->parent(), bounds);
  gfx::Rect bounds_in_another_root =
      gfx::IntersectRects(root_bounds_in_screen, bounds_in_screen);
  const float fraction_in_another_window =
      (bounds_in_another_root.width() * bounds_in_another_root.height()) /
      static_cast<float>(bounds.width() * bounds.height());

  if (fraction_in_another_window > 0) {
    if (!drag_window_controller_) {
      drag_window_controller_.reset(
          new DragWindowController(GetTarget()));
      // Always show the drag phantom on the |another_root| window.
      drag_window_controller_->SetDestinationDisplay(
          Shell::GetScreen()->GetDisplayNearestWindow(another_root));
      drag_window_controller_->Show();
    } else {
      // No animation.
      drag_window_controller_->SetBounds(bounds_in_screen);
    }
    const float phantom_opacity =
      !in_original_root ? 1 : (kMaxOpacity * fraction_in_another_window);
    const float window_opacity =
        in_original_root ? 1 : (kMaxOpacity * (1 - fraction_in_another_window));
    drag_window_controller_->SetOpacity(phantom_opacity);
    GetTarget()->layer()->SetOpacity(window_opacity);
  } else {
    drag_window_controller_.reset();
    GetTarget()->layer()->SetOpacity(1.0f);
  }
}

bool DragWindowResizer::ShouldAllowMouseWarp() {
  return (details().window_component == HTCAPTION) &&
      !::wm::GetTransientParent(GetTarget()) &&
      (GetTarget()->type() == ui::wm::WINDOW_TYPE_NORMAL ||
       GetTarget()->type() == ui::wm::WINDOW_TYPE_PANEL);
}

}  // namespace ash
