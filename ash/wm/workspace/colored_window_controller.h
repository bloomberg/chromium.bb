// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_COLORED_WINDOW_CONTROLLER_H_
#define ASH_WM_WORKSPACE_COLORED_WINDOW_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "base/basictypes.h"

typedef unsigned int SkColor;

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {
namespace internal {

// ColoredWindowController creates a Widget whose layer is LAYER_SOLID_COLOR.
// The Widget is sized to the supplied Window and parented to the specified
// Window. It is used for animations.
class ASH_EXPORT ColoredWindowController {
 public:
  ColoredWindowController(aura::Window* parent, const std::string& window_name);
  ~ColoredWindowController();

  // Changes the background color.
  void SetColor(SkColor color);

  views::Widget* GetWidget();

 private:
  class View;

  // View responsible for rendering the background. This is non-NULL if the
  // widget containing it is deleted. It is owned by the widget.
  View* view_;

  DISALLOW_COPY_AND_ASSIGN(ColoredWindowController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_COLORED_WINDOW_CONTROLLER_H_
