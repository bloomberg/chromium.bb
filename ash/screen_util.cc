// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/screen_util.h"

#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "base/logging.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/screen.h"

namespace ash {

namespace {
DisplayManager* GetDisplayManager() {
  return Shell::GetInstance()->display_manager();
}
}

// static
gfx::Display ScreenUtil::FindDisplayContainingPoint(const gfx::Point& point) {
  return GetDisplayManager()->FindDisplayContainingPoint(point);
}

// static
gfx::Rect ScreenUtil::GetMaximizedWindowBoundsInParent(aura::Window* window) {
  if (GetRootWindowController(window->GetRootWindow())->shelf())
    return GetDisplayWorkAreaBoundsInParent(window);
  else
    return GetDisplayBoundsInParent(window);
}

// static
gfx::Rect ScreenUtil::GetDisplayBoundsInParent(aura::Window* window) {
  return ConvertRectFromScreen(
      window->parent(),
      gfx::Screen::GetScreen()->GetDisplayNearestWindow(window).bounds());
}

// static
gfx::Rect ScreenUtil::GetDisplayWorkAreaBoundsInParent(aura::Window* window) {
  return ConvertRectFromScreen(
      window->parent(),
      gfx::Screen::GetScreen()->GetDisplayNearestWindow(window).work_area());
}

gfx::Rect ScreenUtil::GetShelfDisplayBoundsInRoot(aura::Window* window) {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  if (display_manager->IsInUnifiedMode()) {
    // In unified desktop mode, there is only one shelf in the 1st display.
    const gfx::Display& first =
        display_manager->software_mirroring_display_list()[0];
    float scale =
        static_cast<float>(window->GetRootWindow()->bounds().height()) /
        first.size().height();
    gfx::SizeF size(first.size());
    size.Scale(scale, scale);
    return gfx::Rect(gfx::ToCeiledSize(size));
  } else {
    return window->GetRootWindow()->bounds();
  }
}

// static
gfx::Rect ScreenUtil::ConvertRectToScreen(aura::Window* window,
                                         const gfx::Rect& rect) {
  gfx::Point point = rect.origin();
  aura::client::GetScreenPositionClient(window->GetRootWindow())->
      ConvertPointToScreen(window, &point);
  return gfx::Rect(point, rect.size());
}

// static
gfx::Rect ScreenUtil::ConvertRectFromScreen(aura::Window* window,
                                           const gfx::Rect& rect) {
  gfx::Point point = rect.origin();
  aura::client::GetScreenPositionClient(window->GetRootWindow())->
      ConvertPointFromScreen(window, &point);
  return gfx::Rect(point, rect.size());
}

// static
const gfx::Display& ScreenUtil::GetSecondaryDisplay() {
  DisplayManager* display_manager = GetDisplayManager();
  CHECK_LE(2U, display_manager->GetNumDisplays());
  return display_manager->GetDisplayAt(0).id() ==
                 gfx::Screen::GetScreen()->GetPrimaryDisplay().id()
             ? display_manager->GetDisplayAt(1)
             : display_manager->GetDisplayAt(0);
}

}  // namespace ash
