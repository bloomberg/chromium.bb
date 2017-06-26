// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/ash_switches.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_app_list_view_presenter_impl.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
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

void SetShelfAlignment(ShelfAlignment alignment) {
  test::AshTestBase::GetPrimaryShelf()->SetAlignment(alignment);
}

void EnableMaximizeMode(bool enable) {
  Shell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
      enable);
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

// Tests that the app list initializes in fullscreen with side shelf alignment
// and that the state transitions via text input act properly.
TEST_F(AppListPresenterDelegateTest, SideShelfAlignmentTextStateTransitions) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  EnableFullscreenAppList();
  SetShelfAlignment(ShelfAlignment::SHELF_ALIGNMENT_LEFT);

  // Open the app list with side shelf alignment, then check that it is in
  // fullscreen mode.
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  app_list::AppListView* app_list = app_list_presenter_impl()->GetView();
  EXPECT_TRUE(app_list->is_fullscreen());
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);

  // Enter text in the searchbox, the app list should transition to fullscreen
  // search.
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.PressKey(ui::KeyboardCode::VKEY_0, 0);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_SEARCH);

  // Delete the text in the searchbox, the app list should transition to
  // fullscreen all apps.
  generator.PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);
}

// Tests that the app list initializes in peeking with bottom shelf alignment
// and that the state transitions via text input act properly.
TEST_F(AppListPresenterDelegateTest, BottomShelfAlignmentTextStateTransitions) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  EnableFullscreenAppList();
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  app_list::AppListView* app_list = app_list_presenter_impl()->GetView();
  EXPECT_FALSE(app_list->is_fullscreen());
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::PEEKING);

  // Enter text in the searchbox, this should transition the app list to half
  // state.
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.PressKey(ui::KeyboardCode::VKEY_0, 0);
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::HALF);

  // Empty the searchbox, this should transition the app list to it's previous
  // state.
  generator.PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::PEEKING);
}

// Tests that the app list initializes in fullscreen with maximize mode active
// and that the state transitions via text input act properly.
TEST_F(AppListPresenterDelegateTest, MaximizeModeTextStateTransitions) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  EnableFullscreenAppList();
  EnableMaximizeMode(true);
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  app_list::AppListView* app_list = app_list_presenter_impl()->GetView();
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);

  // Enter text in the searchbox, the app list should transition to fullscreen
  // search.
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.PressKey(ui::KeyboardCode::VKEY_0, 0);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_SEARCH);

  // Delete the text in the searchbox, the app list should transition to
  // fullscreen all apps. generator.PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  generator.PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);
}

// Tests that the app list state responds correctly to maximize mode being
// enabled while the app list is being shown.
TEST_F(AppListPresenterDelegateTest,
       PeekingToFullscreenWhenMaximizeModeIsActive) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  EnableFullscreenAppList();
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  app_list::AppListView* app_list = app_list_presenter_impl()->GetView();
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::PEEKING);
  // Enable maximize mode, this should force the app list to switch to the
  // fullscreen equivalent of the current state.
  EnableMaximizeMode(true);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);
  // Disable maximize mode, the state of the app list should not change.
  EnableMaximizeMode(false);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);
  // Enter text in the searchbox, the app list should transition to fullscreen
  // search.
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.PressKey(ui::KeyboardCode::VKEY_0, 0);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_SEARCH);

  // Delete the text in the searchbox, the app list should transition to
  // fullscreen all apps. generator.PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  generator.PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);
}

// Tests that the app list state responds correctly to maximize mode being
// enabled while the app list is being shown with half launcher.
TEST_F(AppListPresenterDelegateTest, HalfToFullscreenWhenMaximizeModeIsActive) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  EnableFullscreenAppList();
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  app_list::AppListView* app_list = app_list_presenter_impl()->GetView();
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::PEEKING);

  // Enter text in the search box to transition to half app list.
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.PressKey(ui::KeyboardCode::VKEY_0, 0);
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::HALF);

  // Enable maximize mode and force the app list to transition to the fullscreen
  // equivalent of the current state.
  EnableMaximizeMode(true);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_SEARCH);
  generator.PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);
}

// Tests that the app list view handles drag properly in laptop mode.
TEST_F(AppListPresenterDelegateTest, AppListViewDragHandler) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  EnableFullscreenAppList();
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  app_list::AppListView* app_list = app_list_presenter_impl()->GetView();
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::PEEKING);

  ui::test::EventGenerator& generator = GetEventGenerator();
  // Execute a slow short upwards drag this should fail to transition the app
  // list.
  int top_of_app_list = app_list_presenter_impl()
                            ->GetView()
                            ->GetWidget()
                            ->GetWindowBoundsInScreen()
                            .y();
  generator.GestureScrollSequence(gfx::Point(0, top_of_app_list + 20),
                                  gfx::Point(0, top_of_app_list - 20),
                                  base::TimeDelta::FromMilliseconds(500), 50);
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::PEEKING);

  // Execute a long upwards drag, this should transition the app list.
  generator.GestureScrollSequence(gfx::Point(10, top_of_app_list + 20),
                                  gfx::Point(10, 10),
                                  base::TimeDelta::FromMilliseconds(100), 10);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);

  // Execute a short downward drag, this should fail to transition the app list.
  generator.GestureScrollSequence(gfx::Point(10, 10), gfx::Point(10, 100),
                                  base::TimeDelta::FromMilliseconds(100), 10);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);

  // Execute a long downward drag, this should transition the app list.
  generator.GestureScrollSequence(gfx::Point(10, 10), gfx::Point(10, 900),
                                  base::TimeDelta::FromMilliseconds(100), 10);
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::PEEKING);

  // Transition to fullscreen.
  generator.GestureScrollSequence(gfx::Point(10, top_of_app_list + 20),
                                  gfx::Point(10, 10),
                                  base::TimeDelta::FromMilliseconds(100), 10);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);

  // Enter text to transition to |FULLSCREEN_SEARCH|.
  generator.PressKey(ui::KeyboardCode::VKEY_0, 0);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_SEARCH);

  // Execute a short downward drag, this should fail to close the app list.
  generator.GestureScrollSequence(gfx::Point(10, 10), gfx::Point(10, 100),
                                  base::TimeDelta::FromMilliseconds(100), 10);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_SEARCH);

  // Execute a long downward drag, this should close the app list.
  generator.GestureScrollSequence(gfx::Point(10, 10), gfx::Point(10, 900),
                                  base::TimeDelta::FromMilliseconds(100), 10);
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::CLOSED);
}

// Tests that the app list view handles drag properly in maximize mode.
TEST_F(AppListPresenterDelegateTest,
       AppListViewDragHandlerMaximizeModeFromAllApps) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  EnableFullscreenAppList();
  EnableMaximizeMode(true);
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  app_list::AppListView* app_list = app_list_presenter_impl()->GetView();
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);

  ui::test::EventGenerator& generator = GetEventGenerator();
  // Drag down.
  generator.GestureScrollSequence(gfx::Point(0, 0), gfx::Point(0, 720),
                                  base::TimeDelta::FromMilliseconds(100), 10);
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::CLOSED);
}

// Tests that the state of the app list changes properly with drag input from
// fullscreen search.
TEST_F(AppListPresenterDelegateTest,
       AppListViewDragHandlerMaximizeModeFromSearch) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  // Reset the app list.
  EnableFullscreenAppList();
  EnableMaximizeMode(true);
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  app_list::AppListView* app_list = app_list_presenter_impl()->GetView();
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);
  // Type in the search box to transition to |FULLSCREEN_SEARCH|.
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.PressKey(ui::KeyboardCode::VKEY_0, 0);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_SEARCH);
  // Drag down, this should close the app list.
  generator.GestureScrollSequence(gfx::Point(0, 0), gfx::Point(0, 720),
                                  base::TimeDelta::FromMilliseconds(100), 10);
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::CLOSED);
}

// Tests that the bottom shelf background is hidden when the app list is shown
// in laptop mode.
TEST_F(AppListPresenterDelegateTest,
       ShelfBackgroundIsHiddenWhenAppListIsShown) {
  EnableFullscreenAppList();
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  ShelfLayoutManager* shelf_layout_manager =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()))
          ->shelf_layout_manager();
  EXPECT_TRUE(shelf_layout_manager->GetShelfBackgroundType() ==
              ShelfBackgroundType::SHELF_BACKGROUND_DEFAULT);
}

// Tests that the peeking app list closes if the user taps outside its
// bounds.
TEST_F(AppListPresenterDelegateTest, TapAndClickOutsideClosesPeekingAppList) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
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

// Tests that the shelf background displays/hides with bottom shelf
// alignment.
TEST_F(AppListPresenterDelegateTest,
       ShelfBackgroundRespondsToAppListBeingShown) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
   if (Shell::GetAshConfig() == Config::MASH)
     return;
  EnableFullscreenAppList();
  GetPrimaryShelf()->SetAlignment(SHELF_ALIGNMENT_BOTTOM);

  // Show the app list, the shelf background should be transparent.
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  ShelfLayoutManager* shelf_layout_manager =
      GetPrimaryShelf()->shelf_layout_manager();
  EXPECT_EQ(shelf_layout_manager->GetShelfBackgroundType(),
            SHELF_BACKGROUND_DEFAULT);
  app_list_presenter_impl()->Dismiss();

  // Set the alignment to the side and show the app list. The background should
  // show.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::SHELF_ALIGNMENT_LEFT);
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  EXPECT_TRUE(app_list::features::IsFullscreenAppListEnabled());
  EXPECT_FALSE(GetPrimaryShelf()->IsHorizontalAlignment());
  EXPECT_EQ(GetPrimaryShelf()->shelf_layout_manager()->
            GetShelfBackgroundType(),
            SHELF_BACKGROUND_DEFAULT);
}

// Tests that the half app list closes if the user taps outside its bounds.
TEST_F(AppListPresenterDelegateTest, TapAndClickOutsideClosesHalfAppList) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  EnableFullscreenAppList();
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  ui::test::EventGenerator& generator = GetEventGenerator();

  // Transition to half app list by entering text.
  generator.PressKey(ui::KeyboardCode::VKEY_0, 0);
  app_list::AppListView* app_list = app_list_presenter_impl()->GetView();
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::HALF);

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
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::HALF);

  // Clicking inside the bounds doesn't close the app list.
  generator.MoveMouseTo(tap_point);
  generator.ClickLeftButton();
  EXPECT_TRUE(app_list_presenter_impl()->IsVisible());
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::HALF);

  // Tapping outside the bounds closes the app list.
  generator.GestureTapAt(gfx::Point(10, 10));
  EXPECT_FALSE(app_list_presenter_impl()->IsVisible());

  // Reset the app list to half state.
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  generator.PressKey(ui::KeyboardCode::VKEY_0, 0);
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::HALF);

  // Clicking outside the bounds closes the app list.
  generator.MoveMouseTo(gfx::Point(10, 10));
  generator.ClickLeftButton();
  EXPECT_FALSE(app_list_presenter_impl()->IsVisible());
}

}  // namespace ash
