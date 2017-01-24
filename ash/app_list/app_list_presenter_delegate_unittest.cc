// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_app_list_view_presenter_impl.h"
#include "ash/wm/window_util.h"
#include "base/macros.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class AppListPresenterDelegateTest : public test::AshTestBase {
 public:
  AppListPresenterDelegateTest() {}
  ~AppListPresenterDelegateTest() override {}

  app_list::AppListPresenterImpl* app_list_presenter_impl() {
    return &app_list_presenter_impl_;
  }

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    // Make the display big enough to hold the app list.
    UpdateDisplay("1024x768");
  }

 private:
  test::TestAppListViewPresenterImpl app_list_presenter_impl_;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterDelegateTest);
};

// Tests that app launcher hides when focus moves to a normal window.
TEST_F(AppListPresenterDelegateTest, HideOnFocusOut) {
  app_list_presenter_impl()->Show(display_manager()->first_display_id());
  EXPECT_TRUE(app_list_presenter_impl()->GetTargetVisibility());

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window.get());

  EXPECT_FALSE(app_list_presenter_impl()->GetTargetVisibility());
}

// Tests that app launcher remains visible when focus is moved to a different
// window in kShellWindowId_AppListContainer.
TEST_F(AppListPresenterDelegateTest,
       RemainVisibleWhenFocusingToApplistContainer) {
  app_list_presenter_impl()->Show(display_manager()->first_display_id());
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
  app_list_presenter_impl()->Show(display_manager()->first_display_id());
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
  app_list_presenter_impl()->Show(display_manager()->first_display_id());

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

// Tests opening the app launcher on a non-primary display, then deleting the
// display.
TEST_F(AppListPresenterDelegateTest, NonPrimaryDisplay) {
  // Set up a screen with two displays (horizontally adjacent).
  UpdateDisplay("1024x768,1024x768");

  std::vector<WmWindow*> root_windows = WmShell::Get()->GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  WmWindow* secondary_root = root_windows[1];
  EXPECT_EQ("1024,0 1024x768", secondary_root->GetBoundsInScreen().ToString());

  app_list_presenter_impl()->Show(
      secondary_root->GetDisplayNearestWindow().id());
  EXPECT_TRUE(app_list_presenter_impl()->GetTargetVisibility());

  // Remove the secondary display. Shouldn't crash (http://crbug.com/368990).
  UpdateDisplay("1024x768");

  // Updating the displays should close the app list.
  EXPECT_FALSE(app_list_presenter_impl()->GetTargetVisibility());
}

// Tests opening the app launcher on a tiny display that is too small to contain
// it.
TEST_F(AppListPresenterDelegateTest, TinyDisplay) {
  // Set up a screen with a tiny display (height smaller than the app list).
  UpdateDisplay("400x300");

  app_list_presenter_impl()->Show(display_manager()->first_display_id());
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

}  // namespace ash
