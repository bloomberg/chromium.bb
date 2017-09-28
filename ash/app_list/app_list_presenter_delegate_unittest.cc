// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/app_list/test_app_list_presenter_impl.h"
#include "ash/ash_switches.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {
namespace {

constexpr int kAppListBezelMargin = 50;

int64_t GetPrimaryDisplayId() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().id();
}

void SetShelfAlignment(ShelfAlignment alignment) {
  AshTestBase::GetPrimaryShelf()->SetAlignment(alignment);
}

void EnableTabletMode(bool enable) {
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(enable);
}

// Generates a fling.
void FlingUpOrDown(ui::test::EventGenerator& generator,
                   app_list::AppListView* view,
                   bool up) {
  int offset = up ? -100 : 100;
  gfx::Point start_point = view->GetBoundsInScreen().origin();
  gfx::Point target_point = start_point;
  target_point.Offset(0, offset);

  generator.GestureScrollSequence(start_point, target_point,
                                  base::TimeDelta::FromMilliseconds(10), 2);
}

}  // namespace

class AppListPresenterDelegateTest : public AshTestBase,
                                     public testing::WithParamInterface<bool> {
 public:
  AppListPresenterDelegateTest() {}
  ~AppListPresenterDelegateTest() override {}

  TestAppListPresenterImpl* app_list_presenter_impl() {
    return app_list_presenter_impl_.get();
  }

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    if (testing::UnitTest::GetInstance()->current_test_info()->value_param() &&
        GetParam()) {
      EnableFullscreenAppList();
    }

    // Make the display big enough to hold the app list.
    UpdateDisplay("1024x768");

    app_list_presenter_impl_ = base::MakeUnique<TestAppListPresenterImpl>();
  }

  void EnableFullscreenAppList() {
    scoped_feature_list_.InitAndEnableFeature(
        app_list::features::kEnableFullscreenAppList);
  }

 private:
  std::unique_ptr<TestAppListPresenterImpl> app_list_presenter_impl_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterDelegateTest);
};

// TODO(Newcomer): Remove FullscreenAppListPresenterDelegateTest when the
// fullscreen app list becomes default.
class FullscreenAppListPresenterDelegateTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  FullscreenAppListPresenterDelegateTest() {}
  ~FullscreenAppListPresenterDelegateTest() override {}

  TestAppListPresenterImpl* app_list_presenter_impl() {
    return app_list_presenter_impl_.get();
  }

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    scoped_feature_list_.InitAndEnableFeature(
        app_list::features::kEnableFullscreenAppList);

    // Make the display big enough to hold the app list.
    UpdateDisplay("1024x768");

    app_list_presenter_impl_ = base::MakeUnique<TestAppListPresenterImpl>();
  }

  // Whether to run the test with mouse or gesture events.
  bool TestMouseEventParam() { return GetParam(); }

  gfx::Point GetPointOutsideSearchbox() {
    return app_list_presenter_impl_->GetView()->GetBoundsInScreen().origin();
  }

  gfx::Point GetPointInsideSearchbox() {
    return app_list_presenter_impl_->GetView()
        ->search_box_view()
        ->GetBoundsInScreen()
        .CenterPoint();
  }

 private:
  std::unique_ptr<TestAppListPresenterImpl> app_list_presenter_impl_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenAppListPresenterDelegateTest);
};

// Instantiate the Boolean which is used to toggle the Fullscreen app list in
// the parameterized tests.
INSTANTIATE_TEST_CASE_P(, AppListPresenterDelegateTest, testing::Bool());

// Instantiate the Boolean which is used to toggle mouse and touch events in
// the parameterized tests.
INSTANTIATE_TEST_CASE_P(,
                        FullscreenAppListPresenterDelegateTest,
                        testing::Bool());

// Tests that app list hides when focus moves to a normal window.
TEST_P(AppListPresenterDelegateTest, HideOnFocusOut) {
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_TRUE(app_list_presenter_impl()->GetTargetVisibility());

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window.get());
  EXPECT_FALSE(app_list_presenter_impl()->GetTargetVisibility());
}

// Tests that app list remains visible when focus is moved to a different
// window in kShellWindowId_AppListContainer.
TEST_P(AppListPresenterDelegateTest,
       RemainVisibleWhenFocusingToApplistContainer) {
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
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
  // TODO(newcomer): this test needs to be reevaluated for the fullscreen app
  // list (http://crbug.com/759779).
  if (app_list::features::IsFullscreenAppListEnabled())
    return;

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

// Tests that tapping outside the app-list bubble closes it.
TEST_F(AppListPresenterDelegateTest, TapOutsideBubbleClosesBubble) {
  // TODO(newcomer): this test needs to be reevaluated for the fullscreen app
  // list (http://crbug.com/759779).
  if (app_list::features::IsFullscreenAppListEnabled())
    return;

  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  aura::Window* app_window = app_list_presenter_impl()->GetWindow();
  ASSERT_TRUE(app_window);
  gfx::Rect app_window_bounds = app_window->GetBoundsInRootWindow();
  ui::test::EventGenerator& generator = GetEventGenerator();

  // Click on the bubble itself. The bubble should remain visible.
  generator.GestureTapAt(app_window_bounds.CenterPoint());
  EXPECT_TRUE(app_list_presenter_impl()->GetTargetVisibility());

  // Tap outside the bubble. This should close it.
  gfx::Point point_outside =
      gfx::Point(app_window_bounds.right(), app_window_bounds.y()) +
      gfx::Vector2d(10, 0);
  generator.GestureTapAt(point_outside);
  EXPECT_FALSE(app_list_presenter_impl()->GetTargetVisibility());
}

// Tests opening the app list on a secondary display, then deleting the display.
TEST_P(AppListPresenterDelegateTest, NonPrimaryDisplay) {
  // Set up a screen with two displays (horizontally adjacent).
  UpdateDisplay("1024x768,1024x768");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  ASSERT_EQ("1024,0 1024x768", root_windows[1]->GetBoundsInScreen().ToString());

  app_list_presenter_impl()->ShowAndRunLoop(GetSecondaryDisplay().id());
  EXPECT_TRUE(app_list_presenter_impl()->GetTargetVisibility());

  // Remove the secondary display. Shouldn't crash (http://crbug.com/368990).
  UpdateDisplay("1024x768");

  // Updating the displays should close the app list.
  EXPECT_FALSE(app_list_presenter_impl()->GetTargetVisibility());
}

// Tests opening the app list on a tiny display that is too small to contain
// it.
TEST_F(AppListPresenterDelegateTest, TinyDisplay) {
  // TODO(newcomer): this test needs to be reevaluated for the fullscreen app
  // list (http://crbug.com/759779).
  if (app_list::features::IsFullscreenAppListEnabled())
    return;

  // Set up a screen with a tiny display (height smaller than the app list).
  UpdateDisplay("400x300");

  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_TRUE(app_list_presenter_impl()->GetTargetVisibility());

  // The top of the app list should be on-screen (even if the bottom is not).
  // We need to manually calculate the Y coordinate of the top of the app list
  // from the anchor (center) and height. There isn't a bounds rect that gives
  // the actual app list position (the widget bounds include the bubble border
  // which is much bigger than the actual app list size).

  app_list::AppListView* app_list = app_list_presenter_impl()->GetView();
  const int app_list_view_top =
      app_list->anchor_rect().y() - app_list->bounds().height() / 2;
  const int kMinimalAppListMargin = 10;

  EXPECT_GE(app_list_view_top, kMinimalAppListMargin);
}

// Tests that the app list is not draggable in side shelf alignment.
TEST_F(FullscreenAppListPresenterDelegateTest, SideShelfAlignmentDragDisabled) {
  SetShelfAlignment(ShelfAlignment::SHELF_ALIGNMENT_RIGHT);
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  const app_list::AppListView* app_list = app_list_presenter_impl()->GetView();
  EXPECT_TRUE(app_list->is_fullscreen());
  EXPECT_EQ(app_list::AppListView::FULLSCREEN_ALL_APPS,
            app_list->app_list_state());

  // Drag the widget across the screen over an arbitrary 100Ms, this would
  // normally result in the app list transitioning to PEEKING but will now
  // result in no state change.
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(GetPointOutsideSearchbox(),
                                  gfx::Point(10, 900),
                                  base::TimeDelta::FromMilliseconds(100), 10);
  EXPECT_EQ(app_list::AppListView::FULLSCREEN_ALL_APPS,
            app_list->app_list_state());

  // Tap the app list body. This should still close the app list.
  generator.GestureTapAt(GetPointOutsideSearchbox());
  EXPECT_EQ(app_list::AppListView::CLOSED, app_list->app_list_state());
  EXPECT_FALSE(app_list_presenter_impl()->IsVisible());
}

// Tests that the app list initializes in fullscreen with side shelf alignment
// and that the state transitions via text input act properly.
TEST_F(FullscreenAppListPresenterDelegateTest,
       SideShelfAlignmentTextStateTransitions) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  SetShelfAlignment(ShelfAlignment::SHELF_ALIGNMENT_LEFT);

  // Open the app list with side shelf alignment, then check that it is in
  // fullscreen mode.
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
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
TEST_F(FullscreenAppListPresenterDelegateTest,
       BottomShelfAlignmentTextStateTransitions) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
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

// Tests that the app list initializes in fullscreen with tablet mode active
// and that the state transitions via text input act properly.
TEST_F(FullscreenAppListPresenterDelegateTest, TabletModeTextStateTransitions) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  EnableTabletMode(true);
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
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

// Tests that the app list state responds correctly to tablet mode being
// enabled while the app list is being shown.
TEST_F(FullscreenAppListPresenterDelegateTest,
       PeekingToFullscreenWhenTabletModeIsActive) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  app_list::AppListView* app_list = app_list_presenter_impl()->GetView();
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::PEEKING);
  // Enable tablet mode, this should force the app list to switch to the
  // fullscreen equivalent of the current state.
  EnableTabletMode(true);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);
  // Disable tablet mode, the state of the app list should not change.
  EnableTabletMode(false);
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

// Tests that the app list state responds correctly to tablet mode being
// enabled while the app list is being shown with half launcher.
TEST_F(FullscreenAppListPresenterDelegateTest,
       HalfToFullscreenWhenTabletModeIsActive) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  app_list::AppListView* app_list = app_list_presenter_impl()->GetView();
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::PEEKING);

  // Enter text in the search box to transition to half app list.
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.PressKey(ui::KeyboardCode::VKEY_0, 0);
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::HALF);

  // Enable tablet mode and force the app list to transition to the fullscreen
  // equivalent of the current state.
  EnableTabletMode(true);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_SEARCH);
  generator.PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);
}

// Tests that the app list view handles drag properly in laptop mode.
TEST_F(FullscreenAppListPresenterDelegateTest, AppListViewDragHandler) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
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
  gfx::Point start(10, 10);
  gfx::Point end(10, 100);
  generator.GestureScrollSequence(
      start, end,
      generator.CalculateScrollDurationForFlingVelocity(start, end, 1, 100),
      100);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);

  // Execute a long and slow downward drag to switch to peeking.
  start = gfx::Point(10, 200);
  end = gfx::Point(10, 650);
  generator.GestureScrollSequence(
      start, end,
      generator.CalculateScrollDurationForFlingVelocity(start, end, 1, 100),
      100);
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
  start = gfx::Point(10, 10);
  end = gfx::Point(10, 100);
  generator.GestureScrollSequence(
      start, end,
      generator.CalculateScrollDurationForFlingVelocity(start, end, 1, 100),
      100);
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_SEARCH);

  // Execute a long downward drag, this should close the app list.
  generator.GestureScrollSequence(gfx::Point(10, 10), gfx::Point(10, 900),
                                  base::TimeDelta::FromMilliseconds(100), 10);
  EXPECT_EQ(app_list::AppListView::CLOSED, app_list->app_list_state());
  EXPECT_FALSE(app_list_presenter_impl()->IsVisible());
}

// Tests that the app list view handles drag properly in tablet mode.
TEST_F(FullscreenAppListPresenterDelegateTest,
       AppListViewDragHandlerTabletModeFromAllApps) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  EnableTabletMode(true);
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  app_list::AppListView* app_list = app_list_presenter_impl()->GetView();
  EXPECT_EQ(app_list->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);

  ui::test::EventGenerator& generator = GetEventGenerator();
  // Drag down.
  generator.GestureScrollSequence(gfx::Point(0, 0), gfx::Point(0, 720),
                                  base::TimeDelta::FromMilliseconds(100), 10);
  EXPECT_EQ(app_list::AppListView::CLOSED, app_list->app_list_state());
  EXPECT_FALSE(app_list_presenter_impl()->IsVisible());
}

// Tests that the state of the app list changes properly with drag input from
// fullscreen search.
TEST_F(FullscreenAppListPresenterDelegateTest,
       AppListViewDragHandlerTabletModeFromSearch) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  EnableTabletMode(true);
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
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
  EXPECT_EQ(app_list::AppListView::CLOSED, app_list->app_list_state());
  EXPECT_FALSE(app_list_presenter_impl()->IsVisible());
}

// Tests that the bottom shelf background is hidden when the app list is shown
// in laptop mode.
TEST_F(FullscreenAppListPresenterDelegateTest,
       ShelfBackgroundIsHiddenWhenAppListIsShown) {
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  ShelfLayoutManager* shelf_layout_manager =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()))
          ->shelf_layout_manager();
  EXPECT_TRUE(shelf_layout_manager->GetShelfBackgroundType() ==
              ShelfBackgroundType::SHELF_BACKGROUND_APP_LIST);
}

// Tests that the peeking app list closes if the user taps or clicks outside
// its bounds.
TEST_P(FullscreenAppListPresenterDelegateTest,
       TapAndClickOutsideClosesPeekingAppList) {
  const bool test_mouse_event = TestMouseEventParam();
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_EQ(app_list::AppListView::PEEKING,
            app_list_presenter_impl()->GetView()->app_list_state());
  ui::test::EventGenerator& generator = GetEventGenerator();

  // Tapping outside the bounds closes the app list.
  const gfx::Rect peeking_bounds =
      app_list_presenter_impl()->GetView()->GetBoundsInScreen();
  gfx::Point tap_point = peeking_bounds.origin();
  tap_point.Offset(10, -10);
  ASSERT_FALSE(peeking_bounds.Contains(tap_point));

  if (test_mouse_event) {
    generator.MoveMouseTo(tap_point);
    generator.ClickLeftButton();
    generator.ReleaseLeftButton();
  } else {
    generator.GestureTapAt(tap_point);
  }
  EXPECT_FALSE(app_list_presenter_impl()->IsVisible());
}

TEST_P(FullscreenAppListPresenterDelegateTest, LongPressOutsideCloseAppList) {
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_TRUE(app_list_presenter_impl()->IsVisible());

  // |outside_point| is outside the bounds of app list.
  gfx::Point outside_point =
      app_list_presenter_impl()->GetView()->bounds().origin();
  outside_point.Offset(0, -10);

  // Dispatch LONG_PRESS to ash::AppListPresenterDelegate.
  ui::TouchEvent long_press(
      ui::ET_GESTURE_LONG_PRESS, outside_point, base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH));
  GetEventGenerator().Dispatch(&long_press);
  EXPECT_FALSE(app_list_presenter_impl()->IsVisible());
}

TEST_P(FullscreenAppListPresenterDelegateTest,
       TwoFingerTapOutsideCloseAppList) {
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_TRUE(app_list_presenter_impl()->IsVisible());

  // |outside_point| is outside the bounds of app list.
  gfx::Point outside_point =
      app_list_presenter_impl()->GetView()->bounds().origin();
  outside_point.Offset(0, -10);

  // Dispatch TWO_FINGER_TAP to ash::AppListPresenterDelegate.
  ui::TouchEvent two_finger_tap(
      ui::ET_GESTURE_TWO_FINGER_TAP, outside_point, base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH));
  GetEventGenerator().Dispatch(&two_finger_tap);
  EXPECT_FALSE(app_list_presenter_impl()->IsVisible());
}

// Tests that a keypress activates the searchbox and that clearing the
// searchbox, the searchbox remains active.
TEST_F(FullscreenAppListPresenterDelegateTest, KeyPressEnablesSearchBox) {
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  ui::test::EventGenerator& generator = GetEventGenerator();
  app_list::SearchBoxView* search_box_view =
      app_list_presenter_impl()->GetView()->search_box_view();
  EXPECT_FALSE(search_box_view->is_search_box_active());

  // Press any key, the search box should be active.
  generator.PressKey(ui::VKEY_0, 0);
  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Delete the text, the search box should be inactive.
  search_box_view->ClearSearch();
  EXPECT_TRUE(search_box_view->is_search_box_active());
}

// Tests that a tap/click on the AppListView from half launcher returns the
// AppListView to Peeking, and that a tap/click on the AppListView from
// Peeking closes the app list.
TEST_P(FullscreenAppListPresenterDelegateTest,
       StateTransitionsByTapAndClickingAppListBodyFromHalf) {
  const bool test_mouse_event = TestMouseEventParam();
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  app_list::AppListView* app_list_view = app_list_presenter_impl()->GetView();
  app_list::SearchBoxView* search_box_view = app_list_view->search_box_view();
  ui::test::EventGenerator& generator = GetEventGenerator();
  EXPECT_EQ(app_list_view->app_list_state(),
            app_list::AppListView::AppListState::PEEKING);

  // Press a key, the AppListView should transition to half.
  generator.PressKey(ui::KeyboardCode::VKEY_0, 0);
  EXPECT_EQ(app_list_view->app_list_state(),
            app_list::AppListView::AppListState::HALF);
  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Tap outside the search box, the AppListView should transition to Peeking
  // and the search box should be inactive.
  if (test_mouse_event) {
    generator.MoveMouseTo(GetPointOutsideSearchbox());
    generator.ClickLeftButton();
    generator.ReleaseLeftButton();
  } else {
    generator.GestureTapDownAndUp(GetPointOutsideSearchbox());
  }
  EXPECT_EQ(app_list_view->app_list_state(), app_list::AppListView::PEEKING);
  EXPECT_FALSE(search_box_view->is_search_box_active());

  // Tap outside the search box again, the AppListView should hide.
  if (test_mouse_event) {
    generator.MoveMouseTo(GetPointOutsideSearchbox());
    generator.ClickLeftButton();
    generator.ReleaseLeftButton();
  } else {
    generator.GestureTapDownAndUp(GetPointOutsideSearchbox());
  }
  EXPECT_EQ(app_list::AppListView::CLOSED, app_list_view->app_list_state());
  EXPECT_FALSE(app_list_presenter_impl()->IsVisible());
}

// Tests that a tap/click on the AppListView from Fullscreen search returns
// the AppListView to fullscreen all apps, and that a tap/click on the
// AppListView from fullscreen all apps closes the app list.
TEST_P(FullscreenAppListPresenterDelegateTest,
       StateTransitionsByTappingAppListBodyFromFullscreen) {
  const bool test_mouse_event = TestMouseEventParam();
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  app_list::AppListView* app_list_view = app_list_presenter_impl()->GetView();
  app_list::SearchBoxView* search_box_view = app_list_view->search_box_view();
  ui::test::EventGenerator& generator = GetEventGenerator();

  // Execute a long upwards drag, this should transition the app list to
  // fullscreen.
  const int top_of_app_list =
      app_list_view->GetWidget()->GetWindowBoundsInScreen().y();
  generator.GestureScrollSequence(gfx::Point(10, top_of_app_list + 20),
                                  gfx::Point(10, 10),
                                  base::TimeDelta::FromMilliseconds(100), 10);
  EXPECT_EQ(app_list_view->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);

  // Press a key, this should activate the searchbox and transition to
  // fullscreen search.
  generator.PressKey(ui::KeyboardCode::VKEY_0, 0);
  EXPECT_EQ(app_list_view->app_list_state(),
            app_list::AppListView::FULLSCREEN_SEARCH);
  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Tap outside the searchbox, this should deactivate the searchbox and the
  // applistview should return to fullscreen all apps.
  if (test_mouse_event) {
    generator.MoveMouseTo(GetPointOutsideSearchbox());
    generator.ClickLeftButton();
  } else {
    generator.GestureTapDownAndUp(GetPointOutsideSearchbox());
  }
  EXPECT_EQ(app_list_view->app_list_state(),
            app_list::AppListView::FULLSCREEN_ALL_APPS);
  EXPECT_FALSE(search_box_view->is_search_box_active());

  // Tap outside the searchbox again, this should close the applistview.
  if (test_mouse_event) {
    generator.MoveMouseTo(GetPointOutsideSearchbox());
    generator.ClickLeftButton();
  } else {
    generator.GestureTapDownAndUp(GetPointOutsideSearchbox());
  }
  EXPECT_EQ(app_list::AppListView::CLOSED, app_list_view->app_list_state());
  EXPECT_FALSE(app_list_presenter_impl()->IsVisible());
}

// Tests that the searchbox activates when it is tapped and that the widget is
// closed after tapping outside the searchbox.
TEST_P(FullscreenAppListPresenterDelegateTest, TapAndClickEnablesSearchBox) {
  const bool test_mouse_event = TestMouseEventParam();
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  app_list::SearchBoxView* search_box_view =
      app_list_presenter_impl()->GetView()->search_box_view();

  // Tap/Click the search box, it should activate.
  ui::test::EventGenerator& generator = GetEventGenerator();
  if (test_mouse_event) {
    generator.MoveMouseTo(GetPointInsideSearchbox());
    generator.PressLeftButton();
    generator.ReleaseLeftButton();
  } else {
    generator.GestureTapAt(GetPointInsideSearchbox());
  }

  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Tap on the body of the app list, the search box should deactivate.
  if (test_mouse_event) {
    generator.MoveMouseTo(GetPointOutsideSearchbox());
    generator.PressLeftButton();
    generator.ReleaseLeftButton();
  } else {
    generator.GestureTapAt(GetPointOutsideSearchbox());
  }
  EXPECT_FALSE(search_box_view->is_search_box_active());
  EXPECT_TRUE(app_list_presenter_impl()->IsVisible());

  // Tap on the body of the app list again, the app list should hide.
  if (test_mouse_event) {
    generator.PressLeftButton();
    generator.ReleaseLeftButton();
  } else {
    generator.GestureTapAt(GetPointOutsideSearchbox());
  }
  EXPECT_EQ(app_list::AppListView::CLOSED,
            app_list_presenter_impl()->GetView()->app_list_state());
  EXPECT_FALSE(app_list_presenter_impl()->IsVisible());
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
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  ShelfLayoutManager* shelf_layout_manager =
      GetPrimaryShelf()->shelf_layout_manager();
  EXPECT_EQ(shelf_layout_manager->GetShelfBackgroundType(),
            SHELF_BACKGROUND_APP_LIST);
  app_list_presenter_impl()->DismissAndRunLoop();

  // Set the alignment to the side and show the app list. The background
  // should show.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::SHELF_ALIGNMENT_LEFT);
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_TRUE(app_list::features::IsFullscreenAppListEnabled());
  EXPECT_FALSE(GetPrimaryShelf()->IsHorizontalAlignment());
  EXPECT_EQ(GetPrimaryShelf()->shelf_layout_manager()->GetShelfBackgroundType(),
            SHELF_BACKGROUND_APP_LIST);
}

// Tests that the app list in HALF with an active search transitions to PEEKING
// after the body is clicked/tapped.
TEST_P(FullscreenAppListPresenterDelegateTest, HalfToPeekingByClickOrTap) {
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  ui::test::EventGenerator& generator = GetEventGenerator();

  // Transition to half app list by entering text.
  generator.PressKey(ui::KeyboardCode::VKEY_0, 0);
  app_list::AppListView* app_list = app_list_presenter_impl()->GetView();
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::HALF);

  // Click or Tap the app list view body.
  if (TestMouseEventParam()) {
    generator.MoveMouseTo(GetPointOutsideSearchbox());
    generator.ClickLeftButton();
    generator.ReleaseLeftButton();
  } else {
    generator.GestureTapAt(GetPointOutsideSearchbox());
  }
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::PEEKING);

  // Click or Tap the app list view body again.
  if (TestMouseEventParam()) {
    generator.MoveMouseTo(GetPointOutsideSearchbox());
    generator.ClickLeftButton();
    generator.ReleaseLeftButton();
  } else {
    generator.GestureTapAt(GetPointOutsideSearchbox());
  }
  EXPECT_EQ(app_list::AppListView::CLOSED, app_list->app_list_state());
  EXPECT_FALSE(app_list_presenter_impl()->IsVisible());
}

// Tests that the half app list closes if the user taps outside its bounds.
TEST_P(FullscreenAppListPresenterDelegateTest,
       TapAndClickOutsideClosesHalfAppList) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  ui::test::EventGenerator& generator = GetEventGenerator();

  // Transition to half app list by entering text.
  generator.PressKey(ui::KeyboardCode::VKEY_0, 0);
  app_list::AppListView* app_list = app_list_presenter_impl()->GetView();
  EXPECT_EQ(app_list->app_list_state(), app_list::AppListView::HALF);

  // Clicking/tapping outside the bounds closes the app list.
  if (TestMouseEventParam()) {
    generator.MoveMouseTo(gfx::Point(10, 10));
    generator.ClickLeftButton();
  } else {
    generator.GestureTapAt(gfx::Point(10, 10));
  }
  EXPECT_FALSE(app_list_presenter_impl()->IsVisible());
}

// Tests that the search box is set active with a whitespace query and that the
// app list state doesn't transition with a whitespace query.
TEST_F(FullscreenAppListPresenterDelegateTest, WhitespaceQuery) {
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  app_list::AppListView* view = app_list_presenter_impl()->GetView();
  ui::test::EventGenerator& generator = GetEventGenerator();
  EXPECT_FALSE(view->search_box_view()->is_search_box_active());
  EXPECT_EQ(view->app_list_state(), app_list::AppListView::PEEKING);

  // Enter a whitespace query, the searchbox should activate but stay in peeking
  // mode.
  generator.PressKey(ui::VKEY_SPACE, 0);
  EXPECT_TRUE(view->search_box_view()->is_search_box_active());
  EXPECT_EQ(view->app_list_state(), app_list::AppListView::PEEKING);

  // Enter a non-whitespace character, the Searchbox should stay active and go
  // to HALF
  generator.PressKey(ui::VKEY_0, 0);
  EXPECT_TRUE(view->search_box_view()->is_search_box_active());
  EXPECT_EQ(view->app_list_state(), app_list::AppListView::HALF);

  // Delete the non whitespace character, the Searchbox should not deactivate
  // but go to PEEKING
  generator.PressKey(ui::VKEY_BACK, 0);
  EXPECT_TRUE(view->search_box_view()->is_search_box_active());
  EXPECT_EQ(view->app_list_state(), app_list::AppListView::PEEKING);
}

// Tests that an unhandled tap/click in Peeking mode closes the app
// list.
TEST_P(FullscreenAppListPresenterDelegateTest, UnhandledEventOnPeeking) {
  app_list_presenter_impl()->ShowAndRunLoop(GetPrimaryDisplayId());
  app_list::AppListView* view = app_list_presenter_impl()->GetView();
  ASSERT_EQ(view->app_list_state(), app_list::AppListView::PEEKING);

  // Tap or click in the empty space below the searchbox. The app list should
  // close.
  gfx::Point empty_space =
      view->search_box_view()->GetBoundsInScreen().bottom_left();
  empty_space.Offset(0, 10);
  ui::test::EventGenerator& generator = GetEventGenerator();
  if (TestMouseEventParam()) {
    generator.MoveMouseTo(empty_space);
    generator.ClickLeftButton();
    generator.ReleaseLeftButton();
  } else {
    generator.GestureTapAt(empty_space);
  }
  EXPECT_EQ(app_list::AppListView::CLOSED, view->app_list_state());
  EXPECT_FALSE(app_list_presenter_impl()->IsVisible());
}

// Tests that a drag to the bezel from Fullscreen/Peeking will close the app
// list.
TEST_P(FullscreenAppListPresenterDelegateTest,
       DragToBezelClosesAppListFromFullscreenAndPeeking) {
  const bool test_fullscreen = GetParam();
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  app_list::AppListView* view = app_list_presenter_impl()->GetView();
  EXPECT_EQ(app_list::AppListView::PEEKING, view->app_list_state());

  if (test_fullscreen) {
    FlingUpOrDown(GetEventGenerator(), view, true /* up */);
    EXPECT_EQ(app_list::AppListView::FULLSCREEN_ALL_APPS,
              view->app_list_state());
  }

  // Drag the app list to 50 DIPs from the bottom bezel.
  ui::test::EventGenerator& generator = GetEventGenerator();
  const int bezel_y = display::Screen::GetScreen()
                          ->GetDisplayNearestView(view->parent_window())
                          .bounds()
                          .bottom();
  generator.GestureScrollSequence(
      gfx::Point(0, bezel_y - (kAppListBezelMargin + 100)),
      gfx::Point(0, bezel_y - (kAppListBezelMargin)),
      base::TimeDelta::FromMilliseconds(1500), 100);

  EXPECT_EQ(app_list::AppListView::CLOSED, view->app_list_state());
  EXPECT_FALSE(app_list_presenter_impl()->IsVisible());
}

// Tests that a fling from Fullscreen/Peeking closes the app list.
TEST_P(FullscreenAppListPresenterDelegateTest,
       FlingDownClosesAppListFromFullscreenAndPeeking) {
  const bool test_fullscreen = GetParam();
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  app_list::AppListView* view = app_list_presenter_impl()->GetView();
  EXPECT_EQ(app_list::AppListView::PEEKING, view->app_list_state());

  if (test_fullscreen) {
    FlingUpOrDown(GetEventGenerator(), view, true /* up */);
    EXPECT_EQ(app_list::AppListView::FULLSCREEN_ALL_APPS,
              view->app_list_state());
  }

  // Fling down, the app list should close.
  FlingUpOrDown(GetEventGenerator(), view, false /* down */);

  EXPECT_EQ(app_list::AppListView::CLOSED, view->app_list_state());
  EXPECT_FALSE(app_list_presenter_impl()->IsVisible());
}

TEST_F(FullscreenAppListPresenterDelegateTest,
       MouseWheelFromAppListPresenterImplTransitionsAppListState) {
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  app_list::AppListView* view = app_list_presenter_impl()->GetView();
  EXPECT_EQ(app_list::AppListView::PEEKING, view->app_list_state());

  app_list_presenter_impl()->ProcessMouseWheelOffset(-30);

  ASSERT_EQ(app_list::AppListView::FULLSCREEN_ALL_APPS, view->app_list_state());
}

TEST_P(FullscreenAppListPresenterDelegateTest,
       LongUpwardDragInFullscreenShouldNotClose) {
  const bool test_fullscreen_search = GetParam();
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  app_list::AppListView* view = app_list_presenter_impl()->GetView();
  FlingUpOrDown(GetEventGenerator(), view, true);
  EXPECT_EQ(app_list::AppListView::FULLSCREEN_ALL_APPS, view->app_list_state());

  if (test_fullscreen_search) {
    // Enter a character into the searchbox to transition to FULLSCREEN_SEARCH.
    GetEventGenerator().PressKey(ui::VKEY_0, 0);
    EXPECT_EQ(app_list::AppListView::FULLSCREEN_SEARCH, view->app_list_state());
  }

  // Drag from the center of the applist to the top of the screen very slowly.
  // This should not trigger a state transition.
  gfx::Point drag_start = view->GetBoundsInScreen().CenterPoint();
  drag_start.set_x(15);
  gfx::Point drag_end = view->GetBoundsInScreen().top_right();
  drag_end.set_x(15);
  GetEventGenerator().GestureScrollSequence(
      drag_start, drag_end,
      GetEventGenerator().CalculateScrollDurationForFlingVelocity(
          drag_start, drag_end, 1, 1000),
      1000);
  if (test_fullscreen_search)
    EXPECT_EQ(app_list::AppListView::FULLSCREEN_SEARCH, view->app_list_state());
  else
    EXPECT_EQ(app_list::AppListView::FULLSCREEN_ALL_APPS,
              view->app_list_state());
}

// Tests that a drag can not make the app list smaller than the shelf height.
TEST_F(FullscreenAppListPresenterDelegateTest,
       LauncherCannotGetSmallerThanShelf) {
  app_list_presenter_impl()->Show(GetPrimaryDisplayId());
  app_list::AppListView* view = app_list_presenter_impl()->GetView();

  // Try to place the app list 1 px below the shelf, it should stay at shelf
  // height.
  int target_y = GetPrimaryShelf()
                     ->GetShelfViewForTesting()
                     ->GetBoundsInScreen()
                     .top_right()
                     .y();
  const int expected_app_list_y = target_y;
  target_y += 1;
  view->UpdateYPositionAndOpacity(target_y, 1);

  EXPECT_EQ(expected_app_list_y, view->GetBoundsInScreen().top_right().y());
}

}  // namespace ash
