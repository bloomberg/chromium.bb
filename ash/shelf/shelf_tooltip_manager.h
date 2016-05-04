// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_TOOLTIP_MANAGER_H_
#define ASH_SHELF_SHELF_TOOLTIP_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"
#include "ui/views/pointer_watcher.h"

namespace views {
class BubbleDialogDelegateView;
class View;
}

namespace ash {
class ShelfLayoutManager;
class ShelfView;

namespace test {
class ShelfTooltipManagerTest;
class ShelfViewTest;
}

// ShelfTooltipManager manages the tooltip bubble that appears for shelf items.
class ASH_EXPORT ShelfTooltipManager : public ui::EventHandler,
                                       public aura::WindowObserver,
                                       public views::PointerWatcher,
                                       public ShelfLayoutManagerObserver {
 public:
  explicit ShelfTooltipManager(ShelfView* shelf_view);
  ~ShelfTooltipManager() override;

  // Initializes the tooltip manager once the shelf is shown.
  void Init();

  // Closes the tooltip.
  void Close();

  // Returns true if the tooltip is currently visible.
  bool IsVisible() const;

  // Returns the view to which the tooltip bubble is anchored. May be null.
  views::View* GetCurrentAnchorView() const;

  // Show the tooltip bubble for the specified view.
  void ShowTooltip(views::View* view);
  void ShowTooltipWithDelay(views::View* view);

  // Set the timer delay in ms for testing.
  void set_timer_delay_for_test(int timer_delay) { timer_delay_ = timer_delay; }

 protected:
  // ui::EventHandler overrides:
  void OnEvent(ui::Event* event) override;

  // aura::WindowObserver overrides:
  void OnWindowDestroying(aura::Window* window) override;

  // views::PointerWatcher overrides:
  void OnMousePressed(const ui::MouseEvent& event,
                      const gfx::Point& location_in_screen) override;
  void OnTouchPressed(const ui::TouchEvent& event,
                      const gfx::Point& location_in_screen) override;

  // ShelfLayoutManagerObserver overrides:
  void WillDeleteShelf() override;
  void WillChangeVisibilityState(ShelfVisibilityState new_state) override;
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;

 private:
  class ShelfTooltipBubble;
  friend class test::ShelfViewTest;
  friend class test::ShelfTooltipManagerTest;

  int timer_delay_;
  base::OneShotTimer timer_;

  ShelfView* shelf_view_;
  aura::Window* root_window_;
  ShelfLayoutManager* shelf_layout_manager_;
  views::BubbleDialogDelegateView* bubble_;

  base::WeakPtrFactory<ShelfTooltipManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShelfTooltipManager);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_TOOLTIP_MANAGER_H_
