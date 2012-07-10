// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_tooltip_manager.h"

#include "ash/test/ash_test_base.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_util.h"
#include "base/string16.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {

class LauncherTooltipManagerTest : public AshTestBase {
 public:
  LauncherTooltipManagerTest() {}
  virtual ~LauncherTooltipManagerTest() {}

  virtual void SetUp() OVERRIDE {
    ash::test::AshTestBase::SetUp();

    tooltip_manager_.reset(new internal::LauncherTooltipManager(
        SHELF_ALIGNMENT_BOTTOM, Shell::GetInstance()->shelf()));
  }

  void ShowDelayed() {
    dummy_anchor_.reset(new views::View);
    tooltip_manager_->ShowDelayed(dummy_anchor_.get(), string16());
  }

  void ShowImmediately() {
    dummy_anchor_.reset(new views::View);
    tooltip_manager_->ShowImmediately(dummy_anchor_.get(), string16());
  }

  bool TooltipIsVisible() {
    return tooltip_manager_->IsVisible();
  }

  bool IsTimerRunning() {
    return tooltip_manager_->timer_.get() != NULL;
  }

 protected:
  scoped_ptr<views::View> dummy_anchor_;
  scoped_ptr<internal::LauncherTooltipManager> tooltip_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherTooltipManagerTest);
};

TEST_F(LauncherTooltipManagerTest, ShowingBasics) {
  // ShowDelayed() should just start the timer instead of showing immediately.
  ShowDelayed();
  EXPECT_FALSE(TooltipIsVisible());
  EXPECT_TRUE(IsTimerRunning());

  ShowImmediately();
  EXPECT_TRUE(TooltipIsVisible());
  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(LauncherTooltipManagerTest, HideWhenShelfIsHidden) {
  ShowImmediately();
  ASSERT_TRUE(TooltipIsVisible());

  // Create a full-screen window to hide the shelf.
  scoped_ptr<views::Widget> widget(new views::Widget);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);
  widget->SetFullscreen(true);
  widget->Show();

  // Once the shelf is hidden, the tooltip should be invisible.
  ASSERT_EQ(internal::ShelfLayoutManager::HIDDEN,
            Shell::GetInstance()->shelf()->visibility_state());
  EXPECT_FALSE(TooltipIsVisible());

  // Do not show the view if the shelf is hidden.
  ShowImmediately();
  EXPECT_FALSE(TooltipIsVisible());

  // ShowDelayed() doesn't even start the timer for the hidden shelf.
  ShowDelayed();
  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(LauncherTooltipManagerTest, HideWhenShelfIsAutoHide) {
  ShowImmediately();
  ASSERT_TRUE(TooltipIsVisible());

  internal::ShelfLayoutManager* shelf = Shell::GetInstance()->shelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf->UpdateAutoHideState();
  ASSERT_EQ(internal::ShelfLayoutManager::AUTO_HIDE_HIDDEN,
            shelf->auto_hide_state());

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

}  // namespace test
}  // namespace ash
