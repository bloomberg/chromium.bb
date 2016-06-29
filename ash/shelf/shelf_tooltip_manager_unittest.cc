// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_tooltip_manager.h"

#include "ash/common/shelf/shelf_item_delegate_manager.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shelf_test_api.h"
#include "ash/test/shelf_view_test_api.h"
#include "ash/test/test_shelf_item_delegate.h"
#include "base/memory/ptr_util.h"
#include "ui/events/event_constants.h"
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
    shelf_view_ = test::ShelfTestAPI(shelf_).shelf_view();
    tooltip_manager_ = test::ShelfViewTestAPI(shelf_view_).tooltip_manager();
    tooltip_manager_->set_timer_delay_for_test(0);
  }

  bool IsTimerRunning() { return tooltip_manager_->timer_.IsRunning(); }
  views::Widget* GetTooltip() { return tooltip_manager_->bubble_->GetWidget(); }

 protected:
  Shelf* shelf_;
  ShelfView* shelf_view_;
  ShelfTooltipManager* tooltip_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfTooltipManagerTest);
};

TEST_F(ShelfTooltipManagerTest, ShowTooltip) {
  tooltip_manager_->ShowTooltip(shelf_->GetAppListButtonView());
  EXPECT_TRUE(tooltip_manager_->IsVisible());
  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(ShelfTooltipManagerTest, ShowTooltipWithDelay) {
  // ShowTooltipWithDelay should start the timer instead of showing immediately.
  tooltip_manager_->ShowTooltipWithDelay(shelf_->GetAppListButtonView());
  EXPECT_FALSE(tooltip_manager_->IsVisible());
  EXPECT_TRUE(IsTimerRunning());
  RunAllPendingInMessageLoop();
#if !defined(OS_WIN)
  // This check fails on Windows.
  EXPECT_TRUE(tooltip_manager_->IsVisible());
#endif  // !OS_WIN
  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(ShelfTooltipManagerTest, DoNotShowForInvalidView) {
  // The manager should not show or start the timer for a null view.
  tooltip_manager_->ShowTooltip(nullptr);
  EXPECT_FALSE(tooltip_manager_->IsVisible());
  tooltip_manager_->ShowTooltipWithDelay(nullptr);
  EXPECT_FALSE(IsTimerRunning());

  // The manager should not show or start the timer for a non-shelf view.
  views::View view;
  tooltip_manager_->ShowTooltip(&view);
  EXPECT_FALSE(tooltip_manager_->IsVisible());
  tooltip_manager_->ShowTooltipWithDelay(&view);
  EXPECT_FALSE(IsTimerRunning());

  // The manager should start the timer for a view on the shelf.
  ShelfModel* model = shelf_view_->model();
  ShelfItem item;
  item.type = TYPE_APP_SHORTCUT;
  const int index = model->Add(item);
  const ShelfID id = model->items()[index].id;
  Shell::GetInstance()->shelf_item_delegate_manager()->SetShelfItemDelegate(
      id, base::WrapUnique(new TestShelfItemDelegate(nullptr)));
  // Note: There's no easy way to correlate shelf a model index/id to its view.
  tooltip_manager_->ShowTooltipWithDelay(
      shelf_view_->child_at(shelf_view_->child_count() - 1));
  EXPECT_TRUE(IsTimerRunning());

  // Removing the view won't stop the timer, but the tooltip shouldn't be shown.
  model->RemoveItemAt(index);
  EXPECT_TRUE(IsTimerRunning());
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsTimerRunning());
  EXPECT_FALSE(tooltip_manager_->IsVisible());
}

TEST_F(ShelfTooltipManagerTest, HideWhenShelfIsHidden) {
  tooltip_manager_->ShowTooltip(shelf_->GetAppListButtonView());
  ASSERT_TRUE(tooltip_manager_->IsVisible());

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
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // Do not show the view if the shelf is hidden.
  tooltip_manager_->ShowTooltip(shelf_->GetAppListButtonView());
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // ShowTooltipWithDelay doesn't even start the timer for the hidden shelf.
  tooltip_manager_->ShowTooltipWithDelay(shelf_->GetAppListButtonView());
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

  tooltip_manager_->ShowTooltip(shelf_->GetAppListButtonView());
  ASSERT_TRUE(tooltip_manager_->IsVisible());

  shelf_->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_->shelf_layout_manager()->UpdateAutoHideState();
  ASSERT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_->GetAutoHideState());

  // Tooltip visibility change for auto hide may take time.
  EXPECT_TRUE(tooltip_manager_->IsVisible());
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // Do not show the view if the shelf is hidden.
  tooltip_manager_->ShowTooltip(shelf_->GetAppListButtonView());
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // ShowTooltipWithDelay doesn't even run the timer for the hidden shelf.
  tooltip_manager_->ShowTooltipWithDelay(shelf_->GetAppListButtonView());
  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(ShelfTooltipManagerTest, HideForEvents) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  gfx::Rect shelf_bounds = shelf_->shelf_widget()->GetNativeWindow()->bounds();

  // Should hide if the mouse exits the shelf area.
  tooltip_manager_->ShowTooltip(shelf_->GetAppListButtonView());
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator.MoveMouseTo(shelf_bounds.CenterPoint());
  generator.SendMouseExit();
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // Should hide if the mouse is pressed in the shelf area.
  tooltip_manager_->ShowTooltip(shelf_->GetAppListButtonView());
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator.MoveMouseTo(shelf_bounds.CenterPoint());
  generator.PressLeftButton();
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // Should hide for touch events in the shelf.
  tooltip_manager_->ShowTooltip(shelf_->GetAppListButtonView());
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator.set_current_location(shelf_bounds.CenterPoint());
  generator.PressTouch();
  EXPECT_FALSE(tooltip_manager_->IsVisible());

  // Should hide for gesture events in the shelf.
  tooltip_manager_->ShowTooltip(shelf_->GetAppListButtonView());
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator.GestureTapDownAndUp(shelf_bounds.CenterPoint());
  EXPECT_FALSE(tooltip_manager_->IsVisible());
}

TEST_F(ShelfTooltipManagerTest, HideForExternalEvents) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  // TODO(msw): Observe events outside the shelf in mash, to close tooltips.
  aura::Window* shelf_window = shelf_->shelf_widget()->GetNativeWindow();
  bool closes = shelf_window->GetRootWindow() == Shell::GetPrimaryRootWindow();

  // Should hide for touches outside the shelf.
  tooltip_manager_->ShowTooltip(shelf_->GetAppListButtonView());
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator.set_current_location(gfx::Point());
  generator.PressTouch();
  EXPECT_EQ(tooltip_manager_->IsVisible(), !closes);
  generator.ReleaseTouch();

  // Should hide for touch events on the tooltip.
  tooltip_manager_->ShowTooltip(shelf_->GetAppListButtonView());
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator.set_current_location(
      GetTooltip()->GetWindowBoundsInScreen().CenterPoint());
  generator.PressTouch();
  EXPECT_EQ(tooltip_manager_->IsVisible(), !closes);
  generator.ReleaseTouch();

  // Should hide for gestures outside the shelf.
  tooltip_manager_->ShowTooltip(shelf_->GetAppListButtonView());
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator.GestureTapDownAndUp(gfx::Point());
  EXPECT_EQ(tooltip_manager_->IsVisible(), !closes);
}

TEST_F(ShelfTooltipManagerTest, DoNotHideForKeyEvents) {
  ui::test::EventGenerator& generator = GetEventGenerator();

  // Should not hide for key events.
  tooltip_manager_->ShowTooltip(shelf_->GetAppListButtonView());
  ASSERT_TRUE(tooltip_manager_->IsVisible());
  generator.PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_TRUE(tooltip_manager_->IsVisible());
}

}  // namespace test
}  // namespace ash
