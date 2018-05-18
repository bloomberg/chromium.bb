// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_TOUCH_HUD_TOUCH_HUD_RENDERER_H_
#define ASH_COMPONENTS_TOUCH_HUD_TOUCH_HUD_RENDERER_H_

#include <map>
#include <memory>

#include "base/macros.h"

namespace ui {
class PointerEvent;
}

namespace views {
class Widget;
}

namespace touch_hud {
class TouchPointView;

// Renders touch points into a widget.
class TouchHudRenderer {
 public:
  explicit TouchHudRenderer(std::unique_ptr<views::Widget> widget);
  ~TouchHudRenderer();

  // Receives a touch event and draws its touch point.
  void HandleTouchEvent(const ui::PointerEvent& event);

 private:
  friend class TouchHudApplicationTestApi;

  // The widget containing the touch point views.
  std::unique_ptr<views::Widget> widget_;

  // A map of touch ids to TouchPointView.
  std::map<int, TouchPointView*> points_;

  DISALLOW_COPY_AND_ASSIGN(TouchHudRenderer);
};

}  // namespace touch_hud

#endif  // ASH_COMPONENTS_TOUCH_HUD_TOUCH_HUD_RENDERER_H_
