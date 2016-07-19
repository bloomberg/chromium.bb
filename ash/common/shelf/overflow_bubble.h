// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_OVERFLOW_BUBBLE_H_
#define ASH_COMMON_SHELF_OVERFLOW_BUBBLE_H_

#include "base/macros.h"
#include "ui/views/pointer_watcher.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class LocatedEvent;
}

namespace views {
class View;
}

namespace ash {
class OverflowBubbleView;
class ShelfView;
class WmShelf;

// OverflowBubble shows shelf items that won't fit on the main shelf in a
// separate bubble.
class OverflowBubble : public views::PointerWatcher,
                       public views::WidgetObserver {
 public:
  // |wm_shelf| is the shelf that spawns the bubble.
  explicit OverflowBubble(WmShelf* wm_shelf);
  ~OverflowBubble() override;

  // Shows an bubble pointing to |anchor| with |shelf_view| as its content.
  // This |shelf_view| is different than the main shelf's view and only contains
  // the overflow items.
  void Show(views::View* anchor, ShelfView* shelf_view);

  void Hide();

  // Hides the bubble and schedules paint for overflow button.
  void HideBubbleAndRefreshButton();

  bool IsShowing() const { return !!bubble_; }
  ShelfView* shelf_view() { return shelf_view_; }
  OverflowBubbleView* bubble_view() { return bubble_; }

 private:
  void ProcessPressedEvent(const gfx::Point& event_location_in_screen);

  // views::PointerWatcher:
  void OnMousePressed(const ui::MouseEvent& event,
                      const gfx::Point& location_in_screen,
                      views::Widget* target) override;
  void OnTouchPressed(const ui::TouchEvent& event,
                      const gfx::Point& location_in_screen,
                      views::Widget* target) override;

  // Overridden from views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  WmShelf* wm_shelf_;
  OverflowBubbleView* bubble_;  // Owned by views hierarchy.
  views::View* anchor_;         // Owned by ShelfView.
  ShelfView* shelf_view_;       // Owned by |bubble_|.

  DISALLOW_COPY_AND_ASSIGN(OverflowBubble);
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_OVERFLOW_BUBBLE_H_
