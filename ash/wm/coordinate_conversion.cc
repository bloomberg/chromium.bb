// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/coordinate_conversion.h"

#include "ash/display/display_controller.h"
#include "ash/shell.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace wm {

aura::Window* GetRootWindowAt(const gfx::Point& point) {
  const gfx::Display& display =
      Shell::GetScreen()->GetDisplayNearestPoint(point);
  DCHECK(display.is_valid());
  // TODO(yusukes): Move coordinate_conversion.cc and .h to ui/aura/ once
  // GetRootWindowForDisplayId() is moved to aura::Env.
  return Shell::GetInstance()->display_controller()->
      GetRootWindowForDisplayId(display.id());
}

aura::Window* GetRootWindowMatching(const gfx::Rect& rect) {
  const gfx::Display& display = Shell::GetScreen()->GetDisplayMatching(rect);
  return Shell::GetInstance()->display_controller()->
      GetRootWindowForDisplayId(display.id());
}

void ConvertPointToScreen(const aura::Window* window, gfx::Point* point) {
  CHECK(aura::client::GetScreenPositionClient(window->GetRootWindow()));
  aura::client::GetScreenPositionClient(window->GetRootWindow())->
      ConvertPointToScreen(window, point);
}

void ConvertPointFromScreen(const aura::Window* window,
                            gfx::Point* point_in_screen) {
  aura::client::GetScreenPositionClient(window->GetRootWindow())->
      ConvertPointFromScreen(window, point_in_screen);
}

}  // namespace wm
}  // namespace ash
