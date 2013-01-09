// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_tooltip_manager.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_util.h"
#include "base/string16.h"
#include "base/time.h"
#include "ui/aura/root_window.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/events/event_handler.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/views/widget/widget.h"

namespace {

void SetEventTarget(ui::EventTarget* target,
                    ui::Event* event) {
  ui::Event::DispatcherApi dispatch_helper(event);
  dispatch_helper.set_target(target);
}

}

namespace ash {
namespace test {

class LauncherTooltipManagerTest : public AshTestBase {
 public:
  LauncherTooltipManagerTest() {}
  virtual ~LauncherTooltipManagerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    internal::RootWindowController* controller =
        Shell::GetPrimaryRootWindowController();
    tooltip_manager_.reset(new internal::LauncherTooltipManager(
        controller->shelf(),
        controller->launcher()->GetLauncherViewForTest()));
  }

  virtual void TearDown() OVERRIDE {
    tooltip_manager_.reset();
    AshTestBase::TearDown();
  }

  void ShowDelayed() {
    CreateWidget();
    tooltip_manager_->ShowDelayed(dummy_anchor_.get(), string16());
  }

  void ShowImmediately() {
    CreateWidget();
    tooltip_manager_->ShowImmediately(dummy_anchor_.get(), string16());
  }

  bool TooltipIsVisible() {
    return tooltip_manager_->IsVisible();
  }

  bool IsTimerRunning() {
    return tooltip_manager_->timer_.get() != NULL;
  }

  ui::EventHandler* GetEventHandler() {
    return tooltip_manager_.get();
  }

  views::Widget* GetTooltipWidget() {
    return tooltip_manager_->widget_;
  }

 protected:
  scoped_ptr<views::Widget> widget_;
  scoped_ptr<views::View> dummy_anchor_;
  scoped_ptr<internal::LauncherTooltipManager> tooltip_manager_;

 private:
  void CreateWidget() {
    dummy_anchor_.reset(new views::View);

    widget_.reset(new views::Widget);
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.transparent = true;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.parent = Shell::GetContainer(
        Shell::GetPrimaryRootWindow(),
        ash::internal::kShellWindowId_LauncherContainer);

    widget_->Init(params);
    widget_->SetContentsView(dummy_anchor_.get());
  }

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
  params.context = CurrentContext();
  widget->Init(params);
  widget->SetFullscreen(true);
  widget->Show();

  // Once the shelf is hidden, the tooltip should be invisible.
  ASSERT_EQ(
      SHELF_HIDDEN,
      Shell::GetPrimaryRootWindowController()->shelf()->visibility_state());
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

  internal::ShelfLayoutManager* shelf =
      Shell::GetPrimaryRootWindowController()->shelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf->UpdateAutoHideState();
  ASSERT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

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
  ui::EventHandler* event_handler = GetEventHandler();

  // Should not hide for key events.
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE, false);
  SetEventTarget(root_window, &key_event);
  event_handler->OnKeyEvent(&key_event);
  EXPECT_FALSE(key_event.handled());
  EXPECT_TRUE(TooltipIsVisible());

  // Should hide for touch events.
  ui::TouchEvent touch_event(
      ui::ET_TOUCH_PRESSED, gfx::Point(), 0, base::TimeDelta());
  SetEventTarget(root_window, &touch_event);
  event_handler->OnTouchEvent(&touch_event);
  EXPECT_FALSE(touch_event.handled());
  EXPECT_FALSE(TooltipIsVisible());

  // Shouldn't hide if the touch happens on the tooltip.
  ShowImmediately();
  views::Widget* tooltip_widget = GetTooltipWidget();
  SetEventTarget(tooltip_widget->GetNativeWindow(), &touch_event);
  event_handler->OnTouchEvent(&touch_event);
  EXPECT_FALSE(touch_event.handled());
  EXPECT_TRUE(TooltipIsVisible());

  // Should hide for gesture events.
  ui::GestureEvent gesture_event(
      ui::ET_GESTURE_BEGIN, 0, 0, ui::EF_NONE,
      base::TimeDelta::FromMilliseconds(base::Time::Now().ToDoubleT() * 1000),
      ui::GestureEventDetails(ui::ET_GESTURE_BEGIN, 0.0f, 0.0f), 0);
  SetEventTarget(tooltip_widget->GetNativeWindow(), &gesture_event);
  event_handler->OnGestureEvent(&gesture_event);
  EXPECT_FALSE(gesture_event.handled());
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(TooltipIsVisible());
}

TEST_F(LauncherTooltipManagerTest, HideForMouseEvent) {
  ShowImmediately();
  ASSERT_TRUE(TooltipIsVisible());

  aura::RootWindow* root_window = Shell::GetInstance()->GetPrimaryRootWindow();
  ui::EventHandler* event_handler = GetEventHandler();

  gfx::Rect tooltip_rect = GetTooltipWidget()->GetNativeWindow()->bounds();
  ASSERT_FALSE(tooltip_rect.IsEmpty());

  // Shouldn't hide if the mouse is in the tooltip.
  ui::MouseEvent mouse_event(ui::ET_MOUSE_MOVED, tooltip_rect.CenterPoint(),
                             tooltip_rect.CenterPoint(), ui::EF_NONE);
  ui::LocatedEvent::TestApi test_api(&mouse_event);

  SetEventTarget(root_window, &mouse_event);
  event_handler->OnMouseEvent(&mouse_event);
  EXPECT_FALSE(mouse_event.handled());
  EXPECT_TRUE(TooltipIsVisible());

  // Should hide if the mouse is out of the tooltip.
  test_api.set_location(tooltip_rect.origin() + gfx::Vector2d(-1, -1));
  event_handler->OnMouseEvent(&mouse_event);
  EXPECT_FALSE(mouse_event.handled());
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(TooltipIsVisible());
}

}  // namespace test
}  // namespace ash
