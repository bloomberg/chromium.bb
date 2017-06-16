// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/ash_switches.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_app_list_view_presenter_impl.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

int64_t GetPrimaryDisplayId() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().id();
}

}  // namespace

class AppListPresenterDelegateTest : public test::AshTestBase,
                                     public testing::WithParamInterface<bool> {
 public:
  AppListPresenterDelegateTest() {}
  ~AppListPresenterDelegateTest() override {}

  app_list::AppListPresenterImpl* app_list_presenter_impl() {
    return &app_list_presenter_impl_;
  }

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    // If the current test is parameterized.
    if (testing::UnitTest::GetInstance()->current_test_info()->value_param()) {
      test_with_fullscreen_ = GetParam();
      if (test_with_fullscreen_)
        EnableFullscreenAppList();
    }
    // Make the display big enough to hold the app list.
    UpdateDisplay("1024x768");
  }

  void EnableFullscreenAppList() {
    scoped_feature_list_.InitAndEnableFeature(
        app_list::features::kEnableFullscreenAppList);
  }

 private:
  test::TestAppListViewPresenterImpl app_list_presenter_impl_;
  bool test_with_fullscreen_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterDelegateTest);
};

// Instantiate the Boolean which is used to toggle the Fullscreen app list in
// the parameterized tests.
INSTANTIATE_TEST_CASE_P(, AppListPresenterDelegateTest, testing::Bool());

// Tests that app list hides when focus moves to a normal window.
TEST_P(AppListPresenterDelegateTest, HideOnFocusOut) {
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  EXPECT_TRUE(app_list_presenter_impl()->GetTargetVisibility());

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window.get());

  EXPECT_FALSE(app_list_presenter_impl()->GetTargetVisibility());
}

// Tests that app list remains visible when focus is moved to a different
// window in kShellWindowId_AppListContainer.
TEST_P(AppListPresenterDelegateTest,
       RemainVisibleWhenFocusingToApplistContainer) {
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  EXPECT_TRUE(app_list_presenter_impl()->GetTargetVisibility());

  aura::Window* applist_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AppListContainer);
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(0, applist_container));
  wm::ActivateWindow(window.get());

  EXPECT_TRUE(app_list_presenter_impl()->GetTargetVisibility());
}

// Tests that clicking outside the app-list bubble closes it.
TEST_F(AppListPresenterDelegateTest, ClickOutsideBubbleClosesBubble) {
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  aura::Window* app_window = app_list_presenter_impl()->GetWindow();
  ASSERT_TRUE(app_window);
  ui::test::EventGenerator& generator = GetEventGenerator();
  // Click on the bubble itself. The bubble should remain visible.
  generator.MoveMouseToCenterOf(app_window);
  generator.ClickLeftButton();
  EXPECT_TRUE(app_list_presenter_impl()->GetTargetVisibility());

  // Click outside the bubble. This should close it.
  gfx::Rect app_window_bounds = app_window->GetBoundsInRootWindow();
  gfx::Point point_outside =
      gfx::Point(app_window_bounds.right(), app_window_bounds.y()) +
      gfx::Vector2d(10, 0);
  generator.MoveMouseToInHost(point_outside);
  generator.ClickLeftButton();
  EXPECT_FALSE(app_list_presenter_impl()->GetTargetVisibility());
}

// Tests that clicking outside the app-list bubble closes it.
TEST_F(AppListPresenterDelegateTest, TapOutsideBubbleClosesBubble) {
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());

  aura::Window* app_window = app_list_presenter_impl()->GetWindow();
  ASSERT_TRUE(app_window);
  gfx::Rect app_window_bounds = app_window->GetBoundsInRootWindow();

  ui::test::EventGenerator& generator = GetEventGenerator();
  // Click on the bubble itself. The bubble should remain visible.
  generator.GestureTapAt(app_window_bounds.CenterPoint());
  EXPECT_TRUE(app_list_presenter_impl()->GetTargetVisibility());

  // Click outside the bubble. This should close it.
  gfx::Point point_outside =
      gfx::Point(app_window_bounds.right(), app_window_bounds.y()) +
      gfx::Vector2d(10, 0);
  generator.GestureTapAt(point_outside);
  EXPECT_FALSE(app_list_presenter_impl()->GetTargetVisibility());
}

// Tests opening the app list on a non-primary display, then deleting the
// display.
TEST_P(AppListPresenterDelegateTest, NonPrimaryDisplay) {
  // Set up a screen with two displays (horizontally adjacent).
  UpdateDisplay("1024x768,1024x768");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  aura::Window* secondary_root = root_windows[1];
  EXPECT_EQ("1024,0 1024x768", secondary_root->GetBoundsInScreen().ToString());

  app_list_presenter_impl()->Show(display::Screen::GetScreen()
                                      ->GetDisplayNearestWindow(secondary_root)
                                      .id());
  EXPECT_TRUE(app_list_presenter_impl()->GetTargetVisibility());

  // Remove the secondary display. Shouldn't crash (http://crbug.com/368990).
  UpdateDisplay("1024x768");

  // Updating the displays should close the app list.
  EXPECT_FALSE(app_list_presenter_impl()->GetTargetVisibility());
}

// Tests opening the app list on a tiny display that is too small to contain
// it.
TEST_F(AppListPresenterDelegateTest, TinyDisplay) {
  // Set up a screen with a tiny display (height smaller than the app list).
  UpdateDisplay("400x300");

  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  EXPECT_TRUE(app_list_presenter_impl()->GetTargetVisibility());

  // The top of the app list should be on-screen (even if the bottom is not).
  // We need to manually calculate the Y coordinate of the top of the app list
  // from the anchor (center) and height. There isn't a bounds rect that gives
  // the actual app list position (the widget bounds include the bubble border
  // which is much bigger than the actual app list size).

  app_list::AppListView* app_list = app_list_presenter_impl()->GetView();
  int app_list_view_top =
      app_list->anchor_rect().y() - app_list->bounds().height() / 2;
  const int kMinimalAppListMargin = 10;

  EXPECT_GE(app_list_view_top, kMinimalAppListMargin);
}

// Tests that the peeking app list is enlarged to fullscreen after the user
// types in the search box.
TEST_F(AppListPresenterDelegateTest, SnapToFullscreenAfterSearchboxInput) {
  // TODO(newcomer): investigate failure in mash. http://crbug.com/726838.
  if (Shell::GetAshConfig() == Config::MASH)
    return;

  EnableFullscreenAppList();
  UpdateDisplay("1024x768");
  EXPECT_TRUE(app_list::features::IsFullscreenAppListEnabled());
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  app_list::AppListView* app_list = app_list_presenter_impl()->GetView();
  // Check that it is in peeking mode.
  EXPECT_FALSE(app_list->is_fullscreen());

  // Dummy key event to search box.
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.PressKey(ui::KeyboardCode::VKEY_0, 0);
  // Check that it is in fullscreen mode.
  EXPECT_TRUE(app_list->is_fullscreen());
}

// Tests that the peeking app list closes if the user taps outside its
// bounds.
TEST_F(AppListPresenterDelegateTest, TapAndClickOutsideClosesPeekingAppList) {
  EnableFullscreenAppList();

  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  EXPECT_TRUE(app_list_presenter_impl()->GetTargetVisibility());
  ui::test::EventGenerator& generator = GetEventGenerator();

  // Grab the bounds of the search box,
  // which is guaranteed to be inside the app list.
  gfx::Point tap_point = app_list_presenter_impl()
                             ->GetView()
                             ->search_box_widget()
                             ->GetContentsView()
                             ->GetBoundsInScreen()
                             .CenterPoint();

  // Tapping inside the bounds doesn't close the app list.
  generator.GestureTapAt(tap_point);
  EXPECT_TRUE(app_list_presenter_impl()->GetTargetVisibility());

  // Clicking inside the bounds doesn't close the app list.
  generator.MoveMouseTo(tap_point);
  generator.ClickLeftButton();
  EXPECT_TRUE(app_list_presenter_impl()->GetTargetVisibility());

  // Tapping outside the bounds closes the app list.
  tap_point.set_x(tap_point.x() + 750);
  generator.GestureTapAt(tap_point);
  EXPECT_FALSE(app_list_presenter_impl()->GetTargetVisibility());

  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  EXPECT_TRUE(app_list_presenter_impl()->GetTargetVisibility());

  // Clicking outside the bounds closes the app list.
  generator.MoveMouseTo(tap_point);
  generator.ClickLeftButton();
  EXPECT_FALSE(app_list_presenter_impl()->GetTargetVisibility());
}

}  // namespace ash
