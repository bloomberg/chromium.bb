// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/mirror_window_test_api.h"

#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_controller.h"
#include "ash/display/mirror_window_controller.h"
#include "ash/shell.h"
#include "ui/aura/root_window_transformer.h"
#include "ui/gfx/point.h"

namespace ash {
namespace test {

const aura::WindowEventDispatcher* MirrorWindowTestApi::GetDispatcher() const {
  return Shell::GetInstance()->display_controller()->
      mirror_window_controller()->dispatcher_.get();
}

int MirrorWindowTestApi::GetCurrentCursorType() const {
  return Shell::GetInstance()->display_controller()->
      cursor_window_controller()->cursor_type_;
}

const gfx::Point& MirrorWindowTestApi::GetCursorHotPoint() const {
  return Shell::GetInstance()->display_controller()->
      cursor_window_controller()->hot_point_;
}

const aura::Window* MirrorWindowTestApi::GetCursorWindow() const {
  return Shell::GetInstance()->display_controller()->
      cursor_window_controller()->cursor_window_.get();
}

scoped_ptr<aura::RootWindowTransformer>
MirrorWindowTestApi::CreateCurrentRootWindowTransformer() const {
  return Shell::GetInstance()->display_controller()->
      mirror_window_controller()->CreateRootWindowTransformer();
}

}  // namespace test
}  // namespace ash
