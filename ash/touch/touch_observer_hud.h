// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_OBSERVER_HUD_H_
#define ASH_TOUCH_TOUCH_OBSERVER_HUD_H_

#include "ash/shell.h"
#include "ui/base/events/event_handler.h"
#include "ui/gfx/point.h"
#include "ui/views/widget/widget_observer.h"

namespace aura {
class Window;
}

namespace views {
class Label;
class View;
class Widget;
}

namespace ash {
namespace internal {

class TouchHudCanvas;

// An event filter which handles system level gesture events.
class TouchObserverHUD : public ui::EventHandler,
                         public views::WidgetObserver {
 public:
  TouchObserverHUD();
  virtual ~TouchObserverHUD();

  // Changes the display mode (e.g. scale, visibility). Calling this repeatedly
  // cycles between a fixed number of display modes.
  void ChangeToNextMode();

 private:
  void UpdateTouchPointLabel(int index);

  // Overriden from ui::EventHandler:
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  static const int kMaxTouchPoints = 32;

  views::Widget* widget_;
  TouchHudCanvas* canvas_;
  views::View* label_container_;
  views::Label* touch_labels_[kMaxTouchPoints];
  gfx::Point touch_positions_[kMaxTouchPoints];
  ui::EventType touch_status_[kMaxTouchPoints];

  DISALLOW_COPY_AND_ASSIGN(TouchObserverHUD);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_OBSERVER_HUD_H_
