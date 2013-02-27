// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/drag_window_resizer.h"

#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/drag_window_controller.h"
#include "ash/wm/property_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

namespace {

// The maximum opacity of the drag phantom window.
const float kMaxOpacity = 0.8f;

// Returns true if Ash has more than one root window.
bool HasSecondaryRootWindow() {
  return Shell::GetAllRootWindows().size() > 1;
}

// When there are two root windows, returns one of the root windows which is not
// |root_window|. Returns NULL if only one root window exists.
aura::RootWindow* GetAnotherRootWindow(aura::RootWindow* root_window) {
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  if (root_windows.size() < 2)
    return NULL;
  DCHECK_EQ(2U, root_windows.size());
  if (root_windows[0] == root_window)
    return root_windows[1];
  return root_windows[0];
}

}

DragWindowResizer::~DragWindowResizer() {
  Shell* shell = Shell::GetInstance();
  shell->mouse_cursor_filter()->set_mouse_warp_mode(
      MouseCursorEventFilter::WARP_ALWAYS);
  shell->mouse_cursor_filter()->HideSharedEdgeIndicator();

  if (destroyed_)
    *destroyed_ = true;
}

// static
DragWindowResizer* DragWindowResizer::Create(WindowResizer* window_resizer,
                                             aura::Window* window,
                                             const gfx::Point& location,
                                             int window_component) {
  Details details(window, location, window_component);
  return details.is_resizable ?
      new DragWindowResizer(window_resizer, details) : NULL;
}

void DragWindowResizer::Drag(const gfx::Point& location, int event_flags) {
  bool destroyed = false;
  destroyed_ = &destroyed;
  window_resizer_->Drag(location, event_flags);
  if (destroyed)
    return;
  destroyed_ = NULL;
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

void DragWindowResizer::CompleteDrag(int event_flags) {
  window_resizer_->CompleteDrag(event_flags);

  GetTarget()->layer()->SetOpacity(details_.initial_opacity);
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
    const gfx::Rect dst_bounds =
        ScreenAsh::ConvertRectToScreen(GetTarget()->parent(),
                                       GetTarget()->bounds());
    GetTarget()->SetBoundsInScreen(dst_bounds, dst_display);
  }
}

void DragWindowResizer::RevertDrag() {
  window_resizer_->RevertDrag();

  drag_window_controller_.reset();
  GetTarget()->layer()->SetOpacity(details_.initial_opacity);
}

aura::Window* DragWindowResizer::GetTarget() {
  return window_resizer_->GetTarget();
}

DragWindowResizer::DragWindowResizer(WindowResizer* window_resizer,
                                     const Details& details)
    : window_resizer_(window_resizer),
      details_(details),
      destroyed_(NULL) {
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
  if (ShouldAllowMouseWarp()) {
    mouse_cursor_filter->ShowSharedEdgeIndicator(
        details.window->GetRootWindow());
  }
}

void DragWindowResizer::UpdateDragWindow(const gfx::Rect& bounds,
                                         bool in_original_root) {
  if (details_.window_component != HTCAPTION || !ShouldAllowMouseWarp())
    return;

  // It's available. Show a phantom window on the display if needed.
  aura::RootWindow* another_root =
      GetAnotherRootWindow(GetTarget()->GetRootWindow());
  const gfx::Rect root_bounds_in_screen(another_root->GetBoundsInScreen());
  const gfx::Rect bounds_in_screen =
      ScreenAsh::ConvertRectToScreen(GetTarget()->parent(), bounds);
  gfx::Rect bounds_in_another_root =
      gfx::IntersectRects(root_bounds_in_screen, bounds_in_screen);
  const float fraction_in_another_window =
      (bounds_in_another_root.width() * bounds_in_another_root.height()) /
      static_cast<float>(bounds.width() * bounds.height());

  if (fraction_in_another_window > 0) {
    if (!drag_window_controller_.get()) {
      drag_window_controller_.reset(
          new DragWindowController(GetTarget()));
      // Always show the drag phantom on the |another_root| window.
      drag_window_controller_->SetDestinationDisplay(
          Shell::GetScreen()->GetDisplayMatching(
              another_root->GetBoundsInScreen()));
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
  return (details_.window_component == HTCAPTION) &&
      !GetTarget()->transient_parent() &&
      (GetTarget()->type() == aura::client::WINDOW_TYPE_NORMAL);
}

}  // namespace internal
}  // namespace ash
