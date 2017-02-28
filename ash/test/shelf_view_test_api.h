// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_SHELF_VIEW_TEST_API_H_
#define ASH_TEST_SHELF_VIEW_TEST_API_H_

#include "ash/common/shelf/shelf_item_types.h"
#include "base/macros.h"

namespace gfx {
class Rect;
class Size;
}

namespace ui {
class Event;
}

namespace views {
class Button;
class InkDrop;
class View;
}

namespace ash {
class OverflowBubble;
class OverflowButton;
class ShelfButton;
class ShelfButtonPressedMetricTracker;
class ShelfDelegate;
class ShelfTooltipManager;
class ShelfView;

namespace test {

// Use the api in this class to test ShelfView.
class ShelfViewTestAPI {
 public:
  explicit ShelfViewTestAPI(ShelfView* shelf_view);
  ~ShelfViewTestAPI();

  // Number of icons displayed.
  int GetButtonCount();

  // Retrieve the button at |index|, doesn't support the app list button,
  // because the app list button is not a ShelfButton.
  ShelfButton* GetButton(int index);

  // Retrieve the view at |index|.
  views::View* GetViewAt(int index);

  // First visible button index.
  int GetFirstVisibleIndex();

  // Last visible button index.
  int GetLastVisibleIndex();

  // Gets current/ideal bounds for button at |index|.
  const gfx::Rect& GetBoundsByIndex(int index);
  const gfx::Rect& GetIdealBoundsByIndex(int index);

  // Returns true if overflow button is visible.
  bool IsOverflowButtonVisible();

  // Makes shelf view show its overflow bubble.
  void ShowOverflowBubble();

  // Makes shelf view hide its overflow bubble.
  void HideOverflowBubble();

  // Returns true if the overflow bubble is visible.
  bool IsShowingOverflowBubble() const;

  // Sets animation duration in milliseconds for test.
  void SetAnimationDuration(int duration_ms);

  // Runs message loop and waits until all add/remove animations are done.
  void RunMessageLoopUntilAnimationsDone();

  // Closes the app list or context menu if it is running.
  void CloseMenu();

  // An accessor for |shelf_view|.
  ShelfView* shelf_view() { return shelf_view_; }

  // An accessor for the shelf tooltip manager.
  ShelfTooltipManager* tooltip_manager();

  // An accessor for overflow bubble.
  OverflowBubble* overflow_bubble();

  // An accessor for overflow button.
  OverflowButton* overflow_button() const;

  // Returns the preferred size of |shelf_view_|.
  gfx::Size GetPreferredSize();

  // Returns minimum distance before drag starts.
  int GetMinimumDragDistance() const;

  // Wrapper for ShelfView::ButtonPressed.
  void ButtonPressed(views::Button* sender,
                     const ui::Event& event,
                     views::InkDrop* ink_drop);

  // Wrapper for ShelfView::SameDragType.
  bool SameDragType(ShelfItemType typea, ShelfItemType typeb) const;

  // Sets ShelfDelegate.
  void SetShelfDelegate(ShelfDelegate* delegate);

  // Returns re-insertable bounds in screen.
  gfx::Rect GetBoundsForDragInsertInScreen();

  // Returns true if item is ripped off.
  bool IsRippedOffFromShelf();

  // Returns true if an item is ripped off and entered into shelf.
  bool DraggedItemFromOverflowToShelf();

  // An accessor for |shelf_button_pressed_metric_tracker_|.
  ShelfButtonPressedMetricTracker* shelf_button_pressed_metric_tracker();

 private:
  ShelfView* shelf_view_;

  DISALLOW_COPY_AND_ASSIGN(ShelfViewTestAPI);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_SHELF_VIEW_TEST_API_H_
