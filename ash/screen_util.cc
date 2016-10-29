// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/screen_util.h"

#include "ash/aura/wm_shelf_aura.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace ash {

namespace {
display::DisplayManager* GetDisplayManager() {
  return Shell::GetInstance()->display_manager();
}
}

// static
display::Display ScreenUtil::FindDisplayContainingPoint(
    const gfx::Point& point) {
  return GetDisplayManager()->FindDisplayContainingPoint(point);
}

// static
gfx::Rect ScreenUtil::GetMaximizedWindowBoundsInParent(aura::Window* window) {
  aura::Window* root_window = window->GetRootWindow();
  if (GetRootWindowController(root_window)->wm_shelf_aura()->shelf_widget())
    return GetDisplayWorkAreaBoundsInParent(window);
  else
    return GetDisplayBoundsInParent(window);
}

// static
gfx::Rect ScreenUtil::GetDisplayBoundsInParent(aura::Window* window) {
  return ConvertRectFromScreen(
      window->parent(),
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).bounds());
}

// static
gfx::Rect ScreenUtil::GetDisplayWorkAreaBoundsInParent(aura::Window* window) {
  return ConvertRectFromScreen(window->parent(),
                               display::Screen::GetScreen()
                                   ->GetDisplayNearestWindow(window)
                                   .work_area());
}

// static
gfx::Rect ScreenUtil::ConvertRectToScreen(aura::Window* window,
                                          const gfx::Rect& rect) {
  gfx::Point point = rect.origin();
  aura::client::GetScreenPositionClient(window->GetRootWindow())
      ->ConvertPointToScreen(window, &point);
  return gfx::Rect(point, rect.size());
}

// static
gfx::Rect ScreenUtil::ConvertRectFromScreen(aura::Window* window,
                                            const gfx::Rect& rect) {
  gfx::Point point = rect.origin();
  aura::client::GetScreenPositionClient(window->GetRootWindow())
      ->ConvertPointFromScreen(window, &point);
  return gfx::Rect(point, rect.size());
}

// static

}  // namespace ash
