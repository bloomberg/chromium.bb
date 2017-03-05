// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_TEST_ASH_TEST_IMPL_H_
#define ASH_COMMON_TEST_ASH_TEST_IMPL_H_

#include <memory>
#include <string>

#include "ui/display/display_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/window_types.h"

namespace display {
class Display;
}

namespace gfx {
class Rect;
}

namespace ash {

class WindowOwner;
class WmWindow;

// Provides the real implementation of AshTest, see it for details.
class AshTestImpl {
 public:
  virtual ~AshTestImpl() {}

  // Factory function for creating AshTestImpl. Implemention that is used
  // depends upon build dependencies.
  static std::unique_ptr<AshTestImpl> Create();

  // These functions mirror that of AshTest, see it for details.
  virtual void SetUp() = 0;
  virtual void TearDown() = 0;
  virtual void UpdateDisplay(const std::string& display_spec) = 0;
  virtual std::unique_ptr<WindowOwner> CreateTestWindow(
      const gfx::Rect& bounds_in_screen,
      ui::wm::WindowType type,
      int shell_window_id) = 0;
  virtual std::unique_ptr<WindowOwner> CreateToplevelTestWindow(
      const gfx::Rect& bounds_in_screen,
      int shell_window_id) = 0;
  virtual display::Display GetSecondaryDisplay() = 0;
  virtual bool SetSecondaryDisplayPlacement(
      display::DisplayPlacement::Position position,
      int offset) = 0;
  virtual void ConfigureWidgetInitParamsForDisplay(
      WmWindow* window,
      views::Widget::InitParams* init_params) = 0;
  virtual void AddTransientChild(WmWindow* parent, WmWindow* window) = 0;
};

}  // namespace ash

#endif  // ASH_COMMON_TEST_ASH_TEST_IMPL_H_
