// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/mirror_window_test_api.h"

#include "ash/display/cursor_window_controller.h"
#include "ash/display/mirror_window_controller.h"
#include "ash/display/root_window_transformers.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/host/root_window_transformer.h"
#include "ash/shell.h"
#include "ui/gfx/geometry/point.h"

namespace ash {
namespace test {

const aura::WindowTreeHost* MirrorWindowTestApi::GetHost() const {
  aura::Window* window = Shell::GetInstance()
                             ->window_tree_host_manager()
                             ->mirror_window_controller()
                             ->GetWindow();
  return window ? window->GetHost() : NULL;
}

int MirrorWindowTestApi::GetCurrentCursorType() const {
  return Shell::GetInstance()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->cursor_type_;
}

const gfx::Point& MirrorWindowTestApi::GetCursorHotPoint() const {
  return Shell::GetInstance()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->hot_point_;
}

gfx::Point MirrorWindowTestApi::GetCursorHotPointLocationInRootWindow() const {
  return GetCursorWindow()->GetBoundsInRootWindow().origin() +
         GetCursorHotPoint().OffsetFromOrigin();
}

const aura::Window* MirrorWindowTestApi::GetCursorWindow() const {
  return Shell::GetInstance()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->cursor_window_.get();
}

gfx::Point MirrorWindowTestApi::GetCursorLocation() const {
  gfx::Point point = GetCursorWindow()->GetBoundsInScreen().origin();
  const gfx::Point hot_point = GetCursorHotPoint();
  point.Offset(hot_point.x(), hot_point.y());
  return point;
}

}  // namespace test
}  // namespace ash
