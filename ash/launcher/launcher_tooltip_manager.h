// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_TOOLTIP_MANAGER_H_
#define ASH_LAUNCHER_LAUNCHER_TOOLTIP_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/shelf_types.h"
#include "ash/wm/shelf_layout_manager.h"
#include "base/basictypes.h"
#include "base/string16.h"
#include "ui/base/events/event_handler.h"
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
namespace test {
class LauncherTooltipManagerTest;
class LauncherViewTest;
}

namespace internal {
class LauncherView;

// LauncherTooltipManager manages the tooltip balloon poping up on launcher
// items.
class ASH_EXPORT LauncherTooltipManager : public ui::EventHandler,
                                          public ShelfLayoutManager::Observer {
 public:
  LauncherTooltipManager(ShelfLayoutManager* shelf_layout_manager,
                         LauncherView* launcher_view);
  virtual ~LauncherTooltipManager();

  ShelfLayoutManager* shelf_layout_manager() {
    return shelf_layout_manager_;
  }

  // Called when the bubble is closed.
  void OnBubbleClosed(views::BubbleDelegateView* view);

  // Shows the tooltip after a delay.  It also has the appearing animation.
  void ShowDelayed(views::View* anchor, const string16& text);

  // Shows the tooltip immediately.  It omits the appearing animation.
  void ShowImmediately(views::View* anchor, const string16& text);

  // Closes the tooltip.
  void Close();

  // Changes the arrow location of the tooltip in case that the launcher
  // arrangement has changed.
  void UpdateArrowLocation();

  // Resets the timer for the delayed showing |view_|.  If the timer isn't
  // running, it starts a new timer.
  void ResetTimer();

  // Stops the timer for the delayed showing |view_|.
  void StopTimer();

  // Returns true if the tooltip is currently visible.
  bool IsVisible();

protected:
  // ui::EventHandler overrides:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual void OnCancelMode(ui::CancelModeEvent* event) OVERRIDE;

  // ShelfLayoutManager::Observer overrides:
  virtual void WillDeleteShelf() OVERRIDE;
  virtual void WillChangeVisibilityState(
      ShelfVisibilityState new_state) OVERRIDE;
  virtual void OnAutoHideStateChanged(ShelfAutoHideState new_state) OVERRIDE;

 private:
  class LauncherTooltipBubble;
  friend class test::LauncherViewTest;
  friend class test::LauncherTooltipManagerTest;

  void CancelHidingAnimation();
  void CloseSoon();
  void ShowInternal();
  void CreateBubble(views::View* anchor, const string16& text);

  LauncherTooltipBubble* view_;
  views::Widget* widget_;
  views::View* anchor_;
  string16 text_;
  scoped_ptr<base::Timer> timer_;

  ShelfLayoutManager* shelf_layout_manager_;
  LauncherView* launcher_view_;

  DISALLOW_COPY_AND_ASSIGN(LauncherTooltipManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_TOOLTIP_MANAGER_H_
