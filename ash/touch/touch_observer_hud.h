// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_OBSERVER_HUD_H_
#define ASH_TOUCH_TOUCH_OBSERVER_HUD_H_
#pragma once

#include "ash/shell.h"
#include "ui/aura/event_filter.h"
#include "ui/gfx/point.h"

namespace aura {
class MouseEvent;
class KeyEvent;
class Window;
}

namespace views {
class Label;
class Widget;
}

namespace ash {
namespace internal {

class TouchHudCanvas;

// An event filter which handles system level gesture events.
class TouchObserverHUD : public aura::EventFilter {
 public:
  TouchObserverHUD();
  virtual ~TouchObserverHUD();

 private:
  void UpdateTouchPointLabel(int index);

  // Overriden from aura::EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) OVERRIDE;

  static const int kMaxTouchPoints = 32;

  scoped_ptr<views::Widget> widget_;
  TouchHudCanvas* canvas_;
  views::Label* touch_labels_[kMaxTouchPoints];
  gfx::Point touch_positions_[kMaxTouchPoints];
  ui::EventType touch_status_[kMaxTouchPoints];

  DISALLOW_COPY_AND_ASSIGN(TouchObserverHUD);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_OBSERVER_HUD_H_
