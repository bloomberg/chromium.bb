// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/overview/overview_button_tray.h"

#include "ash/ash_switches.h"
#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/user/login_status.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/status_area_widget_test_helper.h"
#include "ash/test/test_session_state_delegate.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "base/command_line.h"
#include "base/test/user_action_tester.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

const char kTrayOverview[] = "Tray_Overview";

OverviewButtonTray* GetTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()->
      overview_button_tray();
}

OverviewButtonTray* GetSecondaryTray() {
  return StatusAreaWidgetTestHelper::GetSecondaryStatusAreaWidget()->
      overview_button_tray();
}

}  // namespace

class OverviewButtonTrayTest : public test::AshTestBase {
 public:
  OverviewButtonTrayTest() {}
  ~OverviewButtonTrayTest() override {}

  void SetUp() override;

  void NotifySessionStateChanged();

 protected:
  views::ImageView* GetImageView(OverviewButtonTray* tray) {
    return tray->icon_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OverviewButtonTrayTest);
};

void OverviewButtonTrayTest::SetUp() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshUseFirstDisplayAsInternal);
  AshTestBase::SetUp();
}

void OverviewButtonTrayTest::NotifySessionStateChanged() {
  GetTray()->SessionStateChanged(
      ash_test_helper()->GetTestSessionStateDelegate()->GetSessionState());
}

// Ensures that creation doesn't cause any crashes and adds the image icon.
TEST_F(OverviewButtonTrayTest, BasicConstruction) {
  EXPECT_TRUE(GetImageView(GetTray()) != NULL);
}

// Test that maximize mode toggle changes visibility.
// OverviewButtonTray should only be visible when MaximizeMode is enabled.
// By default the system should not have MaximizeMode enabled.
TEST_F(OverviewButtonTrayTest, MaximizeModeObserverOnMaximizeModeToggled) {
  ASSERT_FALSE(GetTray()->visible());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  EXPECT_TRUE(GetTray()->visible());

  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);
  EXPECT_FALSE(GetTray()->visible());
}

// Tests that activating this control brings up window selection mode.
TEST_F(OverviewButtonTrayTest, PerformAction) {
  ASSERT_FALSE(Shell::GetInstance()->window_selector_controller()->
      IsSelecting());

  // Overview Mode only works when there is a window
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  ui::GestureEvent tap(
      0, 0, 0, base::TimeDelta(), ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  GetTray()->PerformAction(tap);
  EXPECT_TRUE(Shell::GetInstance()->window_selector_controller()->
      IsSelecting());
}

// Tests that tapping on the control will record the user action Tray_Overview.
TEST_F(OverviewButtonTrayTest, TrayOverviewUserAction) {
  ASSERT_FALSE(
      Shell::GetInstance()->window_selector_controller()->IsSelecting());

  // Tapping on the control when there are no windows (and thus the user cannot
  // enter overview mode) should still record the action.
  base::UserActionTester user_action_tester;
  ui::GestureEvent tap(0, 0, 0, base::TimeDelta(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  GetTray()->PerformAction(tap);
  ASSERT_FALSE(
      Shell::GetInstance()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(1, user_action_tester.GetActionCount(kTrayOverview));

  // With one window present, tapping on the control to enter overview mode
  // should record the user action.
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  GetTray()->PerformAction(tap);
  ASSERT_TRUE(
      Shell::GetInstance()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(2, user_action_tester.GetActionCount(kTrayOverview));

  // Tapping on the control to exit overview mode should record the
  // user action.
  GetTray()->PerformAction(tap);
  ASSERT_FALSE(
      Shell::GetInstance()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(3, user_action_tester.GetActionCount(kTrayOverview));
}

// Tests that a second OverviewButtonTray has been created, and only shows
// when MaximizeMode has been enabled,  when we are using multiple displays.
// By default the DisplayManger is in extended mode.
TEST_F(OverviewButtonTrayTest, DisplaysOnBothDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("400x400,200x200");
  EXPECT_FALSE(GetTray()->visible());
  EXPECT_FALSE(GetSecondaryTray()->visible());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  EXPECT_TRUE(GetTray()->visible());
  EXPECT_TRUE(GetSecondaryTray()->visible());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);
}

// Tests if Maximize Mode is enabled before a secondary display is attached
// that the second OverviewButtonTray should be created in a visible state.
TEST_F(OverviewButtonTrayTest, SecondaryTrayCreatedVisible) {
  if (!SupportsMultipleDisplays())
    return;

  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  UpdateDisplay("400x400,200x200");
  EXPECT_TRUE(GetSecondaryTray()->visible());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);
}

// Tests that the tray loses visibility when a user logs out, and that it
// regains visibility when a user logs back in.
TEST_F(OverviewButtonTrayTest, VisibilityChangesForLoginStatus) {
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  SetUserLoggedIn(false);
  Shell::GetInstance()->UpdateAfterLoginStatusChange(user::LOGGED_IN_NONE);
  EXPECT_FALSE(GetTray()->visible());
  SetUserLoggedIn(true);
  SetSessionStarted(true);
  Shell::GetInstance()->UpdateAfterLoginStatusChange(user::LOGGED_IN_USER);
  EXPECT_TRUE(GetTray()->visible());
  SetUserAddingScreenRunning(true);
  NotifySessionStateChanged();
  EXPECT_FALSE(GetTray()->visible());
  SetUserAddingScreenRunning(false);
  NotifySessionStateChanged();
  EXPECT_TRUE(GetTray()->visible());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);
}

// Tests that the tray only renders as active while selection is ongoing. Any
// dismissal of overview mode clears the active state.
TEST_F(OverviewButtonTrayTest, ActiveStateOnlyDuringOverviewMode) {
  ASSERT_FALSE(
      Shell::GetInstance()->window_selector_controller()->IsSelecting());
  ASSERT_FALSE(GetTray()->draw_background_as_active());

  // Overview Mode only works when there is a window
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  ui::GestureEvent tap(
      0, 0, 0, base::TimeDelta(), ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  GetTray()->PerformAction(tap);
  EXPECT_TRUE(
      Shell::GetInstance()->window_selector_controller()->IsSelecting());
  EXPECT_TRUE(GetTray()->draw_background_as_active());

  Shell::GetInstance()->window_selector_controller()->OnSelectionEnded();
  EXPECT_FALSE(
      Shell::GetInstance()->window_selector_controller()->IsSelecting());
  EXPECT_FALSE(GetTray()->draw_background_as_active());
}

// Test that when a hide animation is aborted via deletion, that the
// OverviewButton is still hidden.
#if defined(OS_WIN) && !defined(USE_ASH)
// TODO(msw): Broken on Windows. http://crbug.com/584038
#define MAYBE_HideAnimationAlwaysCompletes DISABLED_HideAnimationAlwaysCompletes
#else
#define MAYBE_HideAnimationAlwaysCompletes HideAnimationAlwaysCompletes
#endif
TEST_F(OverviewButtonTrayTest, MAYBE_HideAnimationAlwaysCompletes) {
  Shell::GetInstance()
      ->maximize_mode_controller()
      ->EnableMaximizeModeWindowManager(true);

  // Long duration for hide animation, to allow it to be interrupted.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> hide_duration(
      new ui::ScopedAnimationDurationScaleMode(
          ui::ScopedAnimationDurationScaleMode::SLOW_DURATION));
  GetTray()->SetVisible(false);

  // ScreenRotationAnimator copies the current layers, and deletes them upon
  // completion. Allow its animation to complete first.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> rotate_duration(
      new ui::ScopedAnimationDurationScaleMode(
          ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));
  ash::ScreenRotationAnimator(display::Display::InternalDisplayId())
      .Rotate(display::Display::ROTATE_270,
              display::Display::ROTATION_SOURCE_ACTIVE);

  RunAllPendingInMessageLoop();
  EXPECT_FALSE(GetTray()->visible());
}

// Tests that the overview button becomes visible when the user enters
// maximize mode with a system modal window open, and that it hides once
// the user exits maximize mode.
TEST_F(OverviewButtonTrayTest, VisibilityChangesForSystemModalWindow) {
  // TODO(jonross): When CreateTestWindow*() have been unified, use the
  // appropriate method to replace this setup. (crbug.com/483503)
  std::unique_ptr<aura::Window> window(new aura::Window(nullptr));
  window->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_SYSTEM);
  window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  window->Show();
  ParentWindowInPrimaryRootWindow(window.get());

  ASSERT_TRUE(Shell::GetInstance()->IsSystemModalWindowOpen());
  Shell::GetInstance()
      ->maximize_mode_controller()
      ->EnableMaximizeModeWindowManager(true);
  EXPECT_TRUE(GetTray()->visible());
  Shell::GetInstance()
      ->maximize_mode_controller()
      ->EnableMaximizeModeWindowManager(false);
  EXPECT_FALSE(GetTray()->visible());
}

}  // namespace ash
