// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SHELF_SHELF_TOOLTIP_MANAGER_H_
#define MASH_SHELF_SHELF_TOOLTIP_MANAGER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "mash/shelf/shelf_types.h"
#include "ui/events/event_handler.h"

namespace base {
class Timer;
}

namespace views {
class BubbleDelegateView;
class View;
class Widget;
}

namespace mash {
namespace shelf {

class ShelfView;

namespace test {
class ShelfTooltipManagerTest;
class ShelfViewTest;
}

// ShelfTooltipManager manages the tooltip balloon poping up on shelf items.
class ShelfTooltipManager : public ui::EventHandler {
 public:
  explicit ShelfTooltipManager(ShelfView* shelf_view);
  ~ShelfTooltipManager() override;

  // Called when the bubble is closed.
  void OnBubbleClosed(views::BubbleDelegateView* view);

  // Shows the tooltip after a delay.  It also has the appearing animation.
  void ShowDelayed(views::View* anchor, const base::string16& text);

  // Shows the tooltip immediately.  It omits the appearing animation.
  void ShowImmediately(views::View* anchor, const base::string16& text);

  // Closes the tooltip.
  void Close();

  // Resets the timer for the delayed showing |view_|.  If the timer isn't
  // running, it starts a new timer.
  void ResetTimer();

  // Stops the timer for the delayed showing |view_|.
  void StopTimer();

  // Returns true if the tooltip is currently visible.
  bool IsVisible() const;

  // Returns the view to which the tooltip bubble is anchored. May be NULL.
  views::View* GetCurrentAnchorView() const;

  // Create an instant timer for test purposes.
  void CreateZeroDelayTimerForTest();

  ShelfView* shelf_view() const { return shelf_view_; }

protected:
  // ui::EventHandler overrides:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnCancelMode(ui::CancelModeEvent* event) override;

  /* TODO(msw): Restore functionality:
  // ShelfLayoutManagerObserver overrides:
  void WillChangeVisibilityState(ShelfVisibilityState new_state) override;
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;*/

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
  scoped_ptr<base::Timer> timer_;
  ShelfView* shelf_view_;

  base::WeakPtrFactory<ShelfTooltipManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShelfTooltipManager);
};

}  // namespace shelf
}  // namespace mash

#endif  // MASH_SHELF_SHELF_TOOLTIP_MANAGER_H_
