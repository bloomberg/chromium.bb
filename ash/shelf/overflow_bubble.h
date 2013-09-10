// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_OVERFLOW_BUBBLE_H_
#define ASH_SHELF_OVERFLOW_BUBBLE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/events/event_handler.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class LocatedEvent;
}

namespace views {
class View;
}

namespace ash {

class LauncherDelegate;
class LauncherModel;

namespace internal {

class LauncherView;

// OverflowBubble displays the overflown launcher items in a bubble.
class OverflowBubble : public ui::EventHandler,
                       public views::WidgetObserver {
 public:
  OverflowBubble();
  virtual ~OverflowBubble();

  // Shows an bubble pointing to |anchor| with |launcher_view| as its content.
  void Show(views::View* anchor, LauncherView* launcher_view);

  void Hide();

  bool IsShowing() const { return !!bubble_; }
  LauncherView* launcher_view() { return launcher_view_; }

 private:
  void ProcessPressedEvent(ui::LocatedEvent* event);

  // Overridden from ui::EventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  views::View* bubble_;  // Owned by views hierarchy.
  views::View* anchor_;  // Owned by LauncherView.
  LauncherView* launcher_view_;  // Owned by |bubble_|.

  DISALLOW_COPY_AND_ASSIGN(OverflowBubble);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SHELF_OVERFLOW_BUBBLE_H_
