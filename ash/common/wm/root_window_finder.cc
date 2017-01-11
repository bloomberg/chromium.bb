// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/root_window_finder.h"

#include "ash/common/wm_lookup.h"
#include "ash/common/wm_window.h"
#include "ash/root_window_controller.h"
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
  RootWindowController* root_window_controller =
      WmLookup::Get()->GetRootWindowControllerWithDisplayId(display.id());
  return root_window_controller ? root_window_controller->GetWindow() : nullptr;
}

WmWindow* GetRootWindowMatching(const gfx::Rect& rect) {
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayMatching(rect);
  RootWindowController* root_window_controller =
      WmLookup::Get()->GetRootWindowControllerWithDisplayId(display.id());
  return root_window_controller ? root_window_controller->GetWindow() : nullptr;
}

}  // namespace wm
}  // namespace ash
