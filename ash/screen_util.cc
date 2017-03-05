// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/screen_util.h"

#include "ash/common/shelf/wm_shelf.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

// static
gfx::Rect ScreenUtil::GetMaximizedWindowBoundsInParent(aura::Window* window) {
  aura::Window* root_window = window->GetRootWindow();
  if (GetRootWindowController(root_window)->wm_shelf()->shelf_widget())
    return GetDisplayWorkAreaBoundsInParent(window);
  else
    return GetDisplayBoundsInParent(window);
}

// static
gfx::Rect ScreenUtil::GetDisplayBoundsInParent(aura::Window* window) {
  gfx::Rect result =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).bounds();
  ::wm::ConvertRectFromScreen(window->parent(), &result);
  return result;
}

// static
gfx::Rect ScreenUtil::GetDisplayWorkAreaBoundsInParent(aura::Window* window) {
  gfx::Rect result =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).work_area();
  ::wm::ConvertRectFromScreen(window->parent(), &result);
  return result;
}

}  // namespace ash
