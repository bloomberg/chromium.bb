// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/screen_position_controller.h"

#include "ash/display/display_controller.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_properties.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

void ScreenPositionController::ConvertPointToScreen(
    const aura::Window* window,
    gfx::Point* point) {
  const aura::RootWindow* root = window->GetRootWindow();
  aura::Window::ConvertPointToWindow(window, root, point);
  if (DisplayController::IsVirtualScreenCoordinatesEnabled()) {
    const gfx::Point display_origin =
        gfx::Screen::GetDisplayNearestWindow(
            const_cast<aura::RootWindow*>(root)).bounds().origin();
    point->Offset(display_origin.x(), display_origin.y());
  }
}

void ScreenPositionController::ConvertPointFromScreen(
    const aura::Window* window,
    gfx::Point* point) {
  const aura::RootWindow* root = window->GetRootWindow();
  if (DisplayController::IsVirtualScreenCoordinatesEnabled()) {
    const gfx::Point display_origin =
        gfx::Screen::GetDisplayNearestWindow(
            const_cast<aura::RootWindow*>(root)).bounds().origin();
    point->Offset(-display_origin.x(), -display_origin.y());
  }
  aura::Window::ConvertPointToWindow(root, window, point);
}

void ScreenPositionController::SetBounds(
    aura::Window* window,
    const gfx::Rect& bounds) {
  if (!DisplayController::IsVirtualScreenCoordinatesEnabled() ||
      !window->parent()->GetProperty(internal::kUsesScreenCoordinatesKey)) {
    window->SetBounds(bounds);
    return;
  }
  // TODO(oshima): Pick the new root window that most closely shares
  // the bounds.  For a new widget, NativeWidgetAura picks the right
  // root window.
  gfx::Point origin(bounds.origin());
  const gfx::Point display_origin =
      gfx::Screen::GetDisplayNearestWindow(window).bounds().origin();
  origin.Offset(-display_origin.x(), -display_origin.y());
  window->SetBounds(gfx::Rect(origin, bounds.size()));
}

}  // internal
}  // ash
