// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/home_launcher_gesture_handler.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/transform.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class HomeLauncherGestureHandlerTest : public AshTestBase {
 public:
  HomeLauncherGestureHandlerTest() = default;
  ~HomeLauncherGestureHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  }

  // Create a test window and strip it of transient relatives since this does
  // not work with transient windows yet. Set the base transform to identity and
  // the base opacity to opaque for easier testing.
  std::unique_ptr<aura::Window> CreateNonTransientTestWindow() {
    std::unique_ptr<aura::Window> window = CreateTestWindow();
    ::wm::TransientWindowManager* manager =
        ::wm::TransientWindowManager::GetOrCreate(window.get());
    if (manager->transient_parent()) {
      ::wm::TransientWindowManager::GetOrCreate(manager->transient_parent())
          ->RemoveTransientChild(window.get());
    }
    for (auto* window : manager->transient_children())
      manager->RemoveTransientChild(window);

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

// Tests that the gesture handler is available when in tablet mode.
TEST_F(HomeLauncherGestureHandlerTest, Setup) {
  EXPECT_TRUE(GetGestureHandler());

  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
  EXPECT_FALSE(GetGestureHandler());
}

// Tests that the gesture handler will not have a window to act on if there are
// none in the mru list.
TEST_F(HomeLauncherGestureHandlerTest, NeedsOneWindow) {
  GetGestureHandler()->OnPressEvent();
  EXPECT_FALSE(GetGestureHandler()->window());

  auto window = CreateNonTransientTestWindow();
  GetGestureHandler()->OnPressEvent();
  EXPECT_TRUE(GetGestureHandler()->window());
}

// Tests that if there are other visible windows behind the most recent one,
// they get hidden on press event so that the home launcher is visible.
TEST_F(HomeLauncherGestureHandlerTest, ShowWindowsAreHidden) {
  auto window1 = CreateNonTransientTestWindow();
  auto window2 = CreateNonTransientTestWindow();
  auto window3 = CreateNonTransientTestWindow();
  ASSERT_TRUE(window1->IsVisible());
  ASSERT_TRUE(window2->IsVisible());
  ASSERT_TRUE(window3->IsVisible());

  // Test that the most recently activated window is visible, but the others are
  // not.
  ::wm::ActivateWindow(window1.get());
  GetGestureHandler()->OnPressEvent();
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
}

// Tests that the window transform and opacity changes as we scroll.
TEST_F(HomeLauncherGestureHandlerTest, TransformAndOpacityChangesOnScroll) {
  auto window = CreateNonTransientTestWindow();

  GetGestureHandler()->OnPressEvent();
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

// Tests that releasing a drag at the bottom of the work area will return the
// window to its original transform and opacity.
TEST_F(HomeLauncherGestureHandlerTest, BelowHalfReleaseReturnsToOriginalState) {
  UpdateDisplay("400x400");
  auto window1 = CreateNonTransientTestWindow();
  auto window2 = CreateNonTransientTestWindow();
  auto window3 = CreateNonTransientTestWindow();

  ::wm::ActivateWindow(window1.get());
  GetGestureHandler()->OnPressEvent();
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

  // The other windows return to their original visibility.
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
}

// Tests that a drag released at the top half of the work area will minimize the
// window under action.
TEST_F(HomeLauncherGestureHandlerTest, AboveHalfReleaseMinimizesWindow) {
  UpdateDisplay("400x400");
  auto window1 = CreateNonTransientTestWindow();
  auto window2 = CreateNonTransientTestWindow();
  auto window3 = CreateNonTransientTestWindow();

  ::wm::ActivateWindow(window1.get());
  GetGestureHandler()->OnPressEvent();
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

}  // namespace ash
