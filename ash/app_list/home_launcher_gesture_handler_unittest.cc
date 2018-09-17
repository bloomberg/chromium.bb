// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/home_launcher_gesture_handler.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/transform.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace ash {

using Mode = HomeLauncherGestureHandler::Mode;

class HomeLauncherGestureHandlerTest : public AshTestBase {
 public:
  HomeLauncherGestureHandlerTest() = default;
  ~HomeLauncherGestureHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  }

  // Create a test window and set the base transform to identity and
  // the base opacity to opaque for easier testing.
  virtual std::unique_ptr<aura::Window> CreateWindowForTesting() {
    std::unique_ptr<aura::Window> window = CreateTestWindow();

    window->SetTransform(gfx::Transform());
    window->layer()->SetOpacity(1.f);
    return window;
  }

  HomeLauncherGestureHandler* GetGestureHandler() {
    return Shell::Get()->app_list_controller()->home_launcher_gesture_handler();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HomeLauncherGestureHandlerTest);
};

// Tests that the gesture handler will not have a window to act on if there are
// none in the mru list.
TEST_F(HomeLauncherGestureHandlerTest, NeedsOneWindowToShow) {
  GetGestureHandler()->OnPressEvent(Mode::kSwipeUpToShow);
  EXPECT_FALSE(GetGestureHandler()->window());

  auto window = CreateWindowForTesting();
  GetGestureHandler()->OnPressEvent(Mode::kSwipeUpToShow);
  EXPECT_TRUE(GetGestureHandler()->window());
}

// Tests that the gesture handler will not have a window to act on if there are
// none in the mru list, or if they are not minimized.
TEST_F(HomeLauncherGestureHandlerTest, NeedsOneMinimizedWindowToHide) {
  GetGestureHandler()->OnPressEvent(Mode::kSwipeDownToHide);
  EXPECT_FALSE(GetGestureHandler()->window());

  auto window = CreateWindowForTesting();
  GetGestureHandler()->OnPressEvent(Mode::kSwipeDownToHide);
  EXPECT_FALSE(GetGestureHandler()->window());

  wm::GetWindowState(window.get())->Minimize();
  GetGestureHandler()->OnPressEvent(Mode::kSwipeDownToHide);
  EXPECT_TRUE(GetGestureHandler()->window());
}

// Tests that if there are other visible windows behind the most recent one,
// they get hidden on press event so that the home launcher is visible.
TEST_F(HomeLauncherGestureHandlerTest, ShowWindowsAreHidden) {
  auto window1 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  auto window3 = CreateWindowForTesting();
  ASSERT_TRUE(window1->IsVisible());
  ASSERT_TRUE(window2->IsVisible());
  ASSERT_TRUE(window3->IsVisible());

  // Test that the most recently activated window is visible, but the others are
  // not.
  ::wm::ActivateWindow(window1.get());
  GetGestureHandler()->OnPressEvent(Mode::kSwipeUpToShow);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
}

// Tests that the home launcher gestures work with overview mode as expected.
TEST_F(HomeLauncherGestureHandlerTest, OverviewMode) {
  UpdateDisplay("400x448");

  auto window1 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  EXPECT_FALSE(wm::GetWindowState(window1.get())->IsMinimized());
  EXPECT_FALSE(wm::GetWindowState(window2.get())->IsMinimized());

  WindowSelectorController* controller =
      Shell::Get()->window_selector_controller();
  controller->ToggleOverview();
  const int window1_initial_translation =
      window1->transform().To2dTranslation().y();
  const int window2_initial_translation =
      window2->transform().To2dTranslation().y();
  GetGestureHandler()->OnPressEvent(Mode::kSwipeUpToShow);
  EXPECT_FALSE(GetGestureHandler()->window());

  // Tests that while scrolling the window transform changes.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300));
  EXPECT_NE(window1_initial_translation,
            window1->transform().To2dTranslation().y());
  EXPECT_NE(window2_initial_translation,
            window2->transform().To2dTranslation().y());

  // Tests that after releasing at below the halfway point, we remain in
  // overview mode.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300));
  EXPECT_EQ(window1_initial_translation,
            window1->transform().To2dTranslation().y());
  EXPECT_EQ(window2_initial_translation,
            window2->transform().To2dTranslation().y());

  // Tests that after releasing on the bottom half, overview mode has been
  // exited, and the two windows have been minimized to show the home launcher.
  GetGestureHandler()->OnPressEvent(Mode::kSwipeUpToShow);
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 100));
  EXPECT_FALSE(controller->IsSelecting());
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsMinimized());
  EXPECT_TRUE(wm::GetWindowState(window2.get())->IsMinimized());
}

class HomeLauncherModeGestureHandlerTest
    : public HomeLauncherGestureHandlerTest,
      public testing::WithParamInterface<Mode> {
 public:
  HomeLauncherModeGestureHandlerTest() : mode_(GetParam()) {}
  virtual ~HomeLauncherModeGestureHandlerTest() = default;

  // HomeLauncherGestureHandlerTest:
  std::unique_ptr<aura::Window> CreateWindowForTesting() override {
    std::unique_ptr<aura::Window> window =
        HomeLauncherGestureHandlerTest::CreateWindowForTesting();
    if (mode_ == Mode::kSwipeDownToHide)
      wm::GetWindowState(window.get())->Minimize();
    return window;
  }

 protected:
  Mode mode_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HomeLauncherModeGestureHandlerTest);
};

INSTANTIATE_TEST_CASE_P(,
                        HomeLauncherModeGestureHandlerTest,
                        testing::Values(Mode::kSwipeDownToHide,
                                        Mode::kSwipeUpToShow));

// Tests that the window transform and opacity changes as we scroll.
TEST_P(HomeLauncherModeGestureHandlerTest, TransformAndOpacityChangesOnScroll) {
  auto window = CreateWindowForTesting();

  GetGestureHandler()->OnPressEvent(mode_);
  ASSERT_TRUE(GetGestureHandler()->window());

  // Test that on scrolling to a point on the top half of the work area, the
  // window's opacity is between 0 and 0.5 and its transform has changed.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 100));
  const gfx::Transform top_half_transform = window->transform();
  EXPECT_NE(gfx::Transform(), top_half_transform);
  EXPECT_GT(window->layer()->opacity(), 0.f);
  EXPECT_LT(window->layer()->opacity(), 0.5f);

  // Test that on scrolling to a point on the bottom half of the work area, the
  // window's opacity is between 0.5 and 1 and its transform has changed.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300));
  EXPECT_NE(gfx::Transform(), window->transform());
  EXPECT_NE(gfx::Transform(), top_half_transform);
  EXPECT_GT(window->layer()->opacity(), 0.5f);
  EXPECT_LT(window->layer()->opacity(), 1.f);
}

// Tests that releasing a drag at the bottom of the work area will show the
// window.
TEST_P(HomeLauncherModeGestureHandlerTest, BelowHalfShowsWindow) {
  UpdateDisplay("400x400");
  auto window3 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  auto window1 = CreateWindowForTesting();

  GetGestureHandler()->OnPressEvent(mode_);
  ASSERT_TRUE(GetGestureHandler()->window());
  ASSERT_FALSE(window2->IsVisible());
  ASSERT_FALSE(window3->IsVisible());

  // After a scroll the transform and opacity are no longer the identity and 1.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 300));
  EXPECT_NE(gfx::Transform(), window1->transform());
  EXPECT_NE(1.f, window1->layer()->opacity());

  // Tests the transform and opacity have returned to the identity and 1.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300));
  EXPECT_EQ(gfx::Transform(), window1->transform());
  EXPECT_EQ(1.f, window1->layer()->opacity());

  if (mode_ == Mode::kSwipeDownToHide)
    return;

  // The other windows return to their original visibility if mode is swiping
  // up.
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
}

// Tests that a drag released at the top half of the work area will minimize the
// window under action.
TEST_P(HomeLauncherModeGestureHandlerTest, AboveHalfReleaseMinimizesWindow) {
  UpdateDisplay("400x400");
  auto window3 = CreateWindowForTesting();
  auto window2 = CreateWindowForTesting();
  auto window1 = CreateWindowForTesting();

  GetGestureHandler()->OnPressEvent(mode_);
  ASSERT_TRUE(GetGestureHandler()->window());
  ASSERT_FALSE(window2->IsVisible());
  ASSERT_FALSE(window3->IsVisible());

  // Test that |window1| is minimized on release.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 100));
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsMinimized());

  // The rest of the windows remain invisible, to show the home launcher.
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
}

// Tests on swipe up, the transient child of a window which is getting hidden
// will have its opacity and transform altered as well.
TEST_P(HomeLauncherModeGestureHandlerTest, WindowWithTransientChild) {
  UpdateDisplay("400x448");

  // Create a window with a transient child.
  auto parent = CreateWindowForTesting();
  auto child = CreateTestWindow(gfx::Rect(100, 100, 200, 200),
                                aura::client::WINDOW_TYPE_POPUP);
  child->SetTransform(gfx::Transform());
  child->layer()->SetOpacity(1.f);
  ::wm::AddTransientChild(parent.get(), child.get());

  // |parent| should be the window that is getting hidden.
  GetGestureHandler()->OnPressEvent(mode_);
  ASSERT_EQ(parent.get(), GetGestureHandler()->window());

  // Tests that after scrolling to the halfway point, the transient child's
  // opacity and transform are halfway to their final values.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 200));
  EXPECT_LE(0.45f, child->layer()->opacity());
  EXPECT_GE(0.55f, child->layer()->opacity());
  EXPECT_NE(gfx::Transform(), child->transform());

  // Tests that after releasing on the bottom half, the transient child reverts
  // to its original values.
  GetGestureHandler()->OnReleaseEvent(gfx::Point(0, 300));
  EXPECT_EQ(1.0f, child->layer()->opacity());
  EXPECT_EQ(gfx::Transform(), child->transform());
}

// Tests that when tablet mode is ended while in the middle of a scroll session,
// the window is advanced to its end state.
TEST_P(HomeLauncherModeGestureHandlerTest, EndScrollOnTabletModeEnd) {
  auto window = CreateWindowForTesting();

  GetGestureHandler()->OnPressEvent(mode_);
  ASSERT_TRUE(GetGestureHandler()->window());

  // Scroll to a point above the halfway mark of the work area.
  GetGestureHandler()->OnScrollEvent(gfx::Point(0, 50));
  EXPECT_TRUE(GetGestureHandler()->window());
  EXPECT_FALSE(wm::GetWindowState(window.get())->IsMinimized());

  // Tests that on exiting tablet mode, |window| gets minimized and is no longer
  // tracked by the gesture handler.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
  EXPECT_FALSE(GetGestureHandler()->window());
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMinimized());
}

}  // namespace ash
