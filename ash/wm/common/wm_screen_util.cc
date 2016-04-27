// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/common/wm_screen_util.h"

#include "ash/wm/common/wm_root_window_controller.h"
#include "ash/wm/common/wm_window.h"
#include "ui/display/display.h"

namespace ash {
namespace wm {

gfx::Rect GetDisplayWorkAreaBoundsInParent(WmWindow* window) {
  display::Display display = window->GetDisplayNearestWindow();
  return window->GetParent()->ConvertRectFromScreen(display.work_area());
}

gfx::Rect GetDisplayBoundsInParent(WmWindow* window) {
  display::Display display = window->GetDisplayNearestWindow();
  return window->GetParent()->ConvertRectFromScreen(display.bounds());
}

gfx::Rect GetMaximizedWindowBoundsInParent(WmWindow* window) {
  if (window->GetRootWindowController()->HasShelf())
    return GetDisplayWorkAreaBoundsInParent(window);

  return GetDisplayBoundsInParent(window);
}

}  // namespace wm
}  // namespace ash
