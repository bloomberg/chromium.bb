// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOOLTIPS_TOOLTIP_CONTROLLER_H_
#define ASH_TOOLTIPS_TOOLTIP_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/timer.h"
#include "ui/aura/client/tooltip_client.h"
#include "ui/aura/window_observer.h"
#include "ui/base/events/event_handler.h"
#include "ui/gfx/point.h"

namespace aura {
class Window;
namespace client {
class DragDropClient;
}
}

namespace ash {

namespace test {
class TooltipControllerTest;
}  // namespace test

namespace internal {

// TooltipController provides tooltip functionality for aura shell.
class ASH_EXPORT TooltipController : public aura::client::TooltipClient,
                                     public ui::EventHandler,
                                     public aura::WindowObserver {
 public:
  explicit TooltipController(aura::client::DragDropClient* drag_drop_client);
  virtual ~TooltipController();

  // Overridden from aura::client::TooltipClient.
  virtual void UpdateTooltip(aura::Window* target) OVERRIDE;
  virtual void SetTooltipsEnabled(bool enable) OVERRIDE;

  // Overridden from ui::EventHandler.
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual void OnCancelMode(ui::CancelModeEvent* event) OVERRIDE;

  // Overridden from aura::WindowObserver.
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

  gfx::Point mouse_location() const { return curr_mouse_loc_; }

 private:
  friend class ash::test::TooltipControllerTest;

  class Tooltip;

  // Trims the tooltip to fit, setting |text| to the clipped result,
  // |max_width| to the width (in pixels) of the clipped text and |line_count|
  // to the number of lines of text in the tooltip. |x| and |y| give the
  // location of the tooltip in screen coordinates.
  static void TrimTooltipToFit(string16* text,
                               int* max_width,
                               int* line_count,
                               int x,
                               int y);

  void TooltipTimerFired();
  void TooltipShownTimerFired();

  // Updates the tooltip if required (if there is any change in the tooltip
  // text or the aura::Window.
  void UpdateIfRequired();

  // Only used in tests.
  bool IsTooltipVisible();

  bool IsDragDropInProgress();

  // This lazily creates the Tooltip instance so that the tooltip window will
  // be initialized with appropriate drop shadows.
  Tooltip* GetTooltip();

  aura::client::DragDropClient* drag_drop_client_;

  aura::Window* tooltip_window_;
  string16 tooltip_text_;

  // These fields are for tracking state when the user presses a mouse button.
  aura::Window* tooltip_window_at_mouse_press_;
  string16 tooltip_text_at_mouse_press_;
  bool mouse_pressed_;

  scoped_ptr<Tooltip> tooltip_;

  base::RepeatingTimer<TooltipController> tooltip_timer_;

  // Timer to timeout the life of an on-screen tooltip. We hide the tooltip when
  // this timer fires.
  base::OneShotTimer<TooltipController> tooltip_shown_timer_;

  gfx::Point curr_mouse_loc_;

  bool tooltips_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TooltipController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_TOOLTIPS_TOOLTIP_CONTROLLER_H_
