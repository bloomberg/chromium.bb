// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOOLTIPS_TOOLTIP_CONTROLLER_H_
#define ASH_TOOLTIPS_TOOLTIP_CONTROLLER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/timer.h"
#include "ui/aura/client/tooltip_client.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/window_observer.h"
#include "ash/ash_export.h"
#include "ui/gfx/point.h"

namespace aura {
class KeyEvent;
class MouseEvent;
class TouchEvent;
class Window;
}

namespace ash {

namespace test {
class TooltipControllerTest;
}  // namespace test

namespace internal {

// TooltipController provides tooltip functionality for aura shell.
class ASH_EXPORT TooltipController : public aura::client::TooltipClient,
                                            public aura::EventFilter,
                                            public aura::WindowObserver {
 public:
  TooltipController();
  virtual ~TooltipController();

  // Overridden from aura::client::TooltipClient.
  virtual void UpdateTooltip(aura::Window* target) OVERRIDE;

  // Overridden from aura::EventFilter.
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(aura::Window* target,
      aura::GestureEvent* event) OVERRIDE;

  // Overridden from aura::WindowObserver.
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

 private:
  friend class ash::test::TooltipControllerTest;

  class Tooltip;

  void TooltipTimerFired();

  // Updates the tooltip if required (if there is any change in the tooltip
  // text or the aura::Window.
  void UpdateIfRequired();

  aura::Window* tooltip_window_;
  string16 tooltip_text_;
  scoped_ptr<Tooltip> tooltip_;

  base::RepeatingTimer<TooltipController> tooltip_timer_;

  gfx::Point curr_mouse_loc_;

  DISALLOW_COPY_AND_ASSIGN(TooltipController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_TOOLTIPS_TOOLTIP_CONTROLLER_H_
