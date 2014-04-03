// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_TOOLTIP_MANAGER_H_
#define ASH_SHELF_SHELF_TOOLTIP_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_types.h"
#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/rect.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_delegate.h"

namespace base {
class Timer;
}

namespace views {
class BubbleDelegateView;
class Label;
}

namespace ash {
class ShelfView;
class ShelfLayoutManager;

namespace test {
class ShelfTooltipManagerTest;
class ShelfViewTest;
}

// ShelfTooltipManager manages the tooltip balloon poping up on shelf items.
class ASH_EXPORT ShelfTooltipManager : public ui::EventHandler,
                                       public ShelfLayoutManagerObserver {
 public:
  ShelfTooltipManager(ShelfLayoutManager* shelf_layout_manager,
                      ShelfView* shelf_view);
  virtual ~ShelfTooltipManager();

  ShelfLayoutManager* shelf_layout_manager() { return shelf_layout_manager_; }

  // Called when the bubble is closed.
  void OnBubbleClosed(views::BubbleDelegateView* view);

  // Shows the tooltip after a delay.  It also has the appearing animation.
  void ShowDelayed(views::View* anchor, const base::string16& text);

  // Shows the tooltip immediately.  It omits the appearing animation.
  void ShowImmediately(views::View* anchor, const base::string16& text);

  // Closes the tooltip.
  void Close();

  // Changes the arrow location of the tooltip in case that the launcher
  // arrangement has changed.
  void UpdateArrow();

  // Resets the timer for the delayed showing |view_|.  If the timer isn't
  // running, it starts a new timer.
  void ResetTimer();

  // Stops the timer for the delayed showing |view_|.
  void StopTimer();

  // Returns true if the tooltip is currently visible.
  bool IsVisible();

  // Returns the view to which the tooltip bubble is anchored. May be NULL.
  views::View* GetCurrentAnchorView() { return anchor_; }

  // Create an instant timer for test purposes.
  void CreateZeroDelayTimerForTest();

protected:
  // ui::EventHandler overrides:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual void OnCancelMode(ui::CancelModeEvent* event) OVERRIDE;

  // ShelfLayoutManagerObserver overrides:
  virtual void WillDeleteShelf() OVERRIDE;
  virtual void WillChangeVisibilityState(
      ShelfVisibilityState new_state) OVERRIDE;
  virtual void OnAutoHideStateChanged(ShelfAutoHideState new_state) OVERRIDE;

 private:
  class ShelfTooltipBubble;
  friend class test::ShelfViewTest;
  friend class test::ShelfTooltipManagerTest;

  void CancelHidingAnimation();
  void CloseSoon();
  void ShowInternal();
  void CreateBubble(views::View* anchor, const base::string16& text);
  void CreateTimer(int delay_in_ms);

  ShelfTooltipBubble* view_;
  views::Widget* widget_;
  views::View* anchor_;
  base::string16 text_;
  scoped_ptr<base::Timer> timer_;

  ShelfLayoutManager* shelf_layout_manager_;
  ShelfView* shelf_view_;

  base::WeakPtrFactory<ShelfTooltipManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShelfTooltipManager);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_TOOLTIP_MANAGER_H_
