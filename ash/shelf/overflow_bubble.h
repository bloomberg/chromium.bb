// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_OVERFLOW_BUBBLE_H_
#define ASH_SHELF_OVERFLOW_BUBBLE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/events/event_handler.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class LocatedEvent;
}

namespace views {
class View;
}

namespace ash {
class OverflowBubbleView;
class ShelfLayoutManager;
class ShelfView;

// OverflowBubble displays the overflown launcher items in a bubble.
class OverflowBubble : public ui::EventHandler,
                       public views::WidgetObserver {
 public:
  OverflowBubble();
  virtual ~OverflowBubble();

  // Shows an bubble pointing to |anchor| with |shelf_view| as its content.
  void Show(views::View* anchor, ShelfView* shelf_view);

  void Hide();

  // This method as name indicates will hide bubble and schedule paint
  // for overflow button.
  void HideBubbleAndRefreshButton();

  bool IsShowing() const { return !!bubble_; }
  ShelfView* shelf_view() { return shelf_view_; }
  OverflowBubbleView* bubble_view() { return bubble_; }

 private:
  void ProcessPressedEvent(ui::LocatedEvent* event);

  // Overridden from ui::EventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  OverflowBubbleView* bubble_;  // Owned by views hierarchy.
  views::View* anchor_;  // Owned by ShelfView.
  ShelfView* shelf_view_;  // Owned by |bubble_|.

  DISALLOW_COPY_AND_ASSIGN(OverflowBubble);
};

}  // namespace ash

#endif  // ASH_SHELF_OVERFLOW_BUBBLE_H_
