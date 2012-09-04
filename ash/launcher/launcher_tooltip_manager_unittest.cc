// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_tooltip_manager.h"

#include "ash/test/ash_test_base.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_util.h"
#include "base/string16.h"
#include "base/time.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/root_window.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {

class LauncherTooltipManagerTest : public AshTestBase {
 public:
  LauncherTooltipManagerTest() {}
  virtual ~LauncherTooltipManagerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();

    tooltip_manager_.reset(new internal::LauncherTooltipManager(
        SHELF_ALIGNMENT_BOTTOM,
        Shell::GetInstance()->shelf(),
        Shell::GetInstance()->launcher()->GetLauncherViewForTest()));
  }

  virtual void TearDown() OVERRIDE {
    tooltip_manager_.reset();
    AshTestBase::TearDown();
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

  aura::EventFilter* GetEventFilter() {
    return tooltip_manager_.get();
  }

  views::Widget* GetTooltipWidget() {
    return tooltip_manager_->widget_;
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

TEST_F(LauncherTooltipManagerTest, ShouldHideForEvents) {
  ShowImmediately();
  ASSERT_TRUE(TooltipIsVisible());

  aura::RootWindow* root_window = Shell::GetInstance()->GetPrimaryRootWindow();
  aura::EventFilter* event_filter = GetEventFilter();

  // Should not hide for key events.
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(event_filter->PreHandleKeyEvent(root_window, &key_event));
  EXPECT_TRUE(TooltipIsVisible());

  // Should hide for touch events.
  ui::TouchEvent touch_event(
      ui::ET_TOUCH_PRESSED, gfx::Point(), 0, base::TimeDelta());
  EXPECT_EQ(ui::TOUCH_STATUS_UNKNOWN,
            event_filter->PreHandleTouchEvent(root_window, &touch_event));
  EXPECT_FALSE(TooltipIsVisible());

  // Shouldn't hide if the touch happens on the tooltip.
  ShowImmediately();
  views::Widget* tooltip_widget = GetTooltipWidget();
  EXPECT_EQ(ui::TOUCH_STATUS_UNKNOWN,
            event_filter->PreHandleTouchEvent(
                tooltip_widget->GetNativeWindow(), &touch_event));
  EXPECT_TRUE(TooltipIsVisible());

  // Should hide for gesture events.
  ui::GestureEvent gesture_event(
      ui::ET_GESTURE_BEGIN, 0, 0, ui::EF_NONE,
      base::TimeDelta::FromMilliseconds(base::Time::Now().ToDoubleT() * 1000),
      ui::GestureEventDetails(ui::ET_GESTURE_BEGIN, 0.0f, 0.0f), 0);
  EXPECT_EQ(ui::ER_UNHANDLED,
            event_filter->PreHandleGestureEvent(root_window, &gesture_event));
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(TooltipIsVisible());
}

TEST_F(LauncherTooltipManagerTest, HideForMouseEvent) {
  ShowImmediately();
  ASSERT_TRUE(TooltipIsVisible());

  aura::RootWindow* root_window = Shell::GetInstance()->GetPrimaryRootWindow();
  aura::EventFilter* event_filter = GetEventFilter();

  gfx::Rect tooltip_rect = GetTooltipWidget()->GetNativeWindow()->bounds();
  ASSERT_FALSE(tooltip_rect.IsEmpty());

  // Shouldn't hide if the mouse is in the tooltip.
  ui::MouseEvent mouse_event(ui::ET_MOUSE_MOVED, tooltip_rect.CenterPoint(),
                             tooltip_rect.CenterPoint(), ui::EF_NONE);
  ui::LocatedEvent::TestApi test_api(&mouse_event);

  EXPECT_FALSE(event_filter->PreHandleMouseEvent(root_window, &mouse_event));
  EXPECT_TRUE(TooltipIsVisible());

  // Should hide if the mouse is out of the tooltip.
  test_api.set_location(tooltip_rect.origin().Add(gfx::Point(-1, -1)));
  EXPECT_FALSE(event_filter->PreHandleMouseEvent(root_window, &mouse_event));
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(TooltipIsVisible());
}

}  // namespace test
}  // namespace ash
