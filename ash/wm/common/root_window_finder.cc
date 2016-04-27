// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/common/root_window_finder.h"

#include "ash/wm/common/wm_root_window_controller.h"
#include "ash/wm/common/wm_window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace wm {

WmWindow* GetRootWindowAt(const gfx::Point& point) {
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(point);
  DCHECK(display.is_valid());
  WmRootWindowController* root_window_controller =
      WmRootWindowController::GetWithDisplayId(display.id());
  return root_window_controller ? root_window_controller->GetWindow() : nullptr;
}

WmWindow* GetRootWindowMatching(const gfx::Rect& rect) {
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayMatching(rect);
  WmRootWindowController* root_window_controller =
      WmRootWindowController::GetWithDisplayId(display.id());
  return root_window_controller ? root_window_controller->GetWindow() : nullptr;
}

}  // namespace wm
}  // namespace ash
