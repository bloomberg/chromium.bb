// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_tooltip_manager.h"

#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shelf_test_api.h"
#include "ash/test/shelf_view_test_api.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {

class ShelfTooltipManagerTest : public AshTestBase {
 public:
  ShelfTooltipManagerTest() {}
  ~ShelfTooltipManagerTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    shelf_ = Shelf::ForPrimaryDisplay();
    ShelfView* shelf_view = test::ShelfTestAPI(shelf_).shelf_view();
    tooltip_manager_ = test::ShelfViewTestAPI(shelf_view).tooltip_manager();
  }

  void ShowDelayed() {
    CreateWidget();
    tooltip_manager_->ShowTooltipWithDelay(dummy_anchor_.get());
  }

  void ShowImmediately() {
    CreateWidget();
    tooltip_manager_->ShowTooltip(dummy_anchor_.get());
  }

  bool TooltipIsVisible() { return tooltip_manager_->IsVisible(); }
  bool IsTimerRunning() { return tooltip_manager_->timer_.IsRunning(); }

  aura::Window* GetTooltipWindow() {
    return tooltip_manager_->bubble_->GetWidget()->GetNativeWindow();
  }

 protected:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<views::View> dummy_anchor_;

  Shelf* shelf_;
  ShelfTooltipManager* tooltip_manager_;

 private:
  void CreateWidget() {
    dummy_anchor_.reset(new views::View);
    widget_.reset(new views::Widget);
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.parent = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                        ash::kShellWindowId_ShelfContainer);
    widget_->Init(params);
    widget_->SetContentsView(dummy_anchor_.get());
  }

  DISALLOW_COPY_AND_ASSIGN(ShelfTooltipManagerTest);
};

TEST_F(ShelfTooltipManagerTest, ShowingBasics) {
  // ShowDelayed() should just start the timer instead of showing immediately.
  ShowDelayed();
  EXPECT_FALSE(TooltipIsVisible());
  EXPECT_TRUE(IsTimerRunning());

  ShowImmediately();
  EXPECT_TRUE(TooltipIsVisible());
  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(ShelfTooltipManagerTest, HideWhenShelfIsHidden) {
  ShowImmediately();
  ASSERT_TRUE(TooltipIsVisible());

  // Create a full-screen window to hide the shelf.
  std::unique_ptr<views::Widget> widget(new views::Widget);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = CurrentContext();
  widget->Init(params);
  widget->SetFullscreen(true);
  widget->Show();

  // Once the shelf is hidden, the tooltip should be invisible.
  ASSERT_EQ(SHELF_HIDDEN, shelf_->shelf_layout_manager()->visibility_state());
  EXPECT_FALSE(TooltipIsVisible());

  // Do not show the view if the shelf is hidden.
  ShowImmediately();
  EXPECT_FALSE(TooltipIsVisible());

  // ShowDelayed() doesn't even start the timer for the hidden shelf.
  ShowDelayed();
  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(ShelfTooltipManagerTest, HideWhenShelfIsAutoHide) {
  // Create a visible window so auto-hide behavior is enforced.
  views::Widget* dummy = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  dummy->Init(params);
  dummy->Show();

  ShowImmediately();
  ASSERT_TRUE(TooltipIsVisible());

  ShelfLayoutManager* shelf_layout_manager = shelf_->shelf_layout_manager();
  shelf_layout_manager->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_layout_manager->UpdateAutoHideState();
  ASSERT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_layout_manager->auto_hide_state());

  // Tooltip visibility change for auto hide may take time.
  EXPECT_TRUE(TooltipIsVisible());
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(TooltipIsVisible());

  // Do not show the view if the shelf is hidden.
  ShowImmediately();
  EXPECT_FALSE(TooltipIsVisible());

  // ShowDelayed doesn't even run the timer for the hidden shelf.
  ShowDelayed();
  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(ShelfTooltipManagerTest, HideForEvents) {
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  gfx::Rect shelf_bounds = shelf_->shelf_widget()->GetNativeWindow()->bounds();

  // Should hide if the mouse exits the shelf area.
  ShowImmediately();
  ASSERT_TRUE(TooltipIsVisible());
  generator.MoveMouseTo(shelf_bounds.CenterPoint());
  generator.SendMouseExit();
  EXPECT_FALSE(TooltipIsVisible());

  // Should hide if the mouse is pressed in the shelf area.
  ShowImmediately();
  ASSERT_TRUE(TooltipIsVisible());
  generator.MoveMouseTo(shelf_bounds.CenterPoint());
  generator.PressLeftButton();
  EXPECT_FALSE(TooltipIsVisible());

  // Should hide for touch events in the shelf.
  ShowImmediately();
  ASSERT_TRUE(TooltipIsVisible());
  generator.set_current_location(shelf_bounds.CenterPoint());
  generator.PressTouch();
  EXPECT_FALSE(TooltipIsVisible());

  // Should hide for gesture events in the shelf.
  ShowImmediately();
  ASSERT_TRUE(TooltipIsVisible());
  generator.GestureTapDownAndUp(shelf_bounds.CenterPoint());
  EXPECT_FALSE(TooltipIsVisible());
}

TEST_F(ShelfTooltipManagerTest, HideForExternalEvents) {
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  // TODO(msw): Observe events outside the shelf in mash, to close tooltips.
  aura::Window* shelf_window = shelf_->shelf_widget()->GetNativeWindow();
  bool closes = shelf_window->GetRootWindow() == Shell::GetPrimaryRootWindow();

  // Should hide for touches outside the shelf.
  ShowImmediately();
  ASSERT_TRUE(TooltipIsVisible());
  generator.set_current_location(gfx::Point());
  generator.PressTouch();
  EXPECT_EQ(TooltipIsVisible(), !closes);

  // Should hide for touch events on the tooltip.
  ShowImmediately();
  ASSERT_TRUE(TooltipIsVisible());
  generator.set_current_location(GetTooltipWindow()->bounds().CenterPoint());
  generator.PressTouch();
  EXPECT_EQ(TooltipIsVisible(), !closes);

  // Should hide for gestures outside the shelf.
  ShowImmediately();
  ASSERT_TRUE(TooltipIsVisible());
  generator.GestureTapDownAndUp(gfx::Point());
  EXPECT_EQ(TooltipIsVisible(), !closes);
}

TEST_F(ShelfTooltipManagerTest, DoNotHideForKeyEvents) {
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  // Should not hide for key events.
  ShowImmediately();
  ASSERT_TRUE(TooltipIsVisible());
  generator.PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_TRUE(TooltipIsVisible());
}

}  // namespace test
}  // namespace ash
