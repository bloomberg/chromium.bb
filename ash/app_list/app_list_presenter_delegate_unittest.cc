// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "ash/app_list/pagination_model.h"
#include "ash/app_list/presenter/app_list_presenter_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/test/app_list_test_model.h"
#include "ash/app_list/test/app_list_test_view_delegate.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/test/apps_grid_view_test_api.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/interfaces/app_list_view.mojom.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/shell_test_api.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/root_window_finder.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller_test_api.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/public/keyboard_switches.h"
#include "ui/keyboard/test/keyboard_test_util.h"
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
  // Avoid |TabletModeController::OnGetSwitchStates| from disabling tablet mode
  // again at the end of |TabletModeController::TabletModeController|.
  base::RunLoop().RunUntilIdle();

  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(enable);

  // The app list will be shown automatically when tablet mode is enabled (Home
  // launcher flag is enabled). Wait here for the animation complete.
  base::RunLoop().RunUntilIdle();
}

// Generates a fling.
void FlingUpOrDown(ui::test::EventGenerator* generator,
                   app_list::AppListView* view,
                   bool up) {
  int offset = up ? -100 : 100;
  gfx::Point start_point = view->GetBoundsInScreen().origin();
  gfx::Point target_point = start_point;
  target_point.Offset(0, offset);

  generator->GestureScrollSequence(start_point, target_point,
                                   base::TimeDelta::FromMilliseconds(10), 2);
}

}  // namespace

class AppListPresenterDelegateTest : public AshTestBase,
                                     public testing::WithParamInterface<bool> {
 public:
  AppListPresenterDelegateTest() = default;
  ~AppListPresenterDelegateTest() override = default;

  // testing::Test:
  void SetUp() override {
    app_list::AppListView::SetShortAnimationForTesting(true);
    AshTestBase::SetUp();

    // Zeros state changes expected UI behavior. Most test cases in this suite
    // are the expected UI behavior with zero state being disabled.
    // TODO(jennyz): Add new test cases for zero state, crbug.com/925195.
    scoped_feature_list_.InitAndDisableFeature(
        app_list_features::kEnableZeroStateSuggestions);
    // Make the display big enough to hold the app list.
    UpdateDisplay("1024x768");
  }

  void TearDown() override {
    AshTestBase::TearDown();
    app_list::AppListView::SetShortAnimationForTesting(false);
  }

  // Whether to run the test with mouse or gesture events.
  bool TestMouseEventParam() { return GetParam(); }

  gfx::Point GetPointOutsideSearchbox() {
    return GetAppListView()->GetBoundsInScreen().origin();
  }

  gfx::Point GetPointInsideSearchbox() {
    return GetAppListView()
        ->search_box_view()
        ->GetBoundsInScreen()
        .CenterPoint();
  }

  app_list::AppListView* GetAppListView() {
    return GetAppListTestHelper()->GetAppListView();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterDelegateTest);
};

// Used to test app_list behavior with a populated apps_grid
class PopulatedAppListTest : public AshTestBase,
                             public testing::WithParamInterface<bool> {
 public:
  PopulatedAppListTest() = default;
  ~PopulatedAppListTest() override = default;

  void SetUp() override {
    app_list::AppListView::SetShortAnimationForTesting(true);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();

    // Make the display big enough to hold the app list.
    UpdateDisplay("1024x768");

    app_list_test_delegate_ =
        std::make_unique<app_list::test::AppListTestViewDelegate>();

    app_list_test_model_ = app_list_test_delegate_->GetTestModel();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    app_list::AppListView::SetShortAnimationForTesting(false);
  }

 protected:
  void CreateAndOpenAppList() {
    app_list_view_ = new app_list::AppListView(app_list_test_delegate_.get());
    app_list::AppListView::InitParams params;
    params.parent = CurrentContext();
    app_list_view_->Initialize(params);
  }

  void InitializeAppsGrid() {
    if (!app_list_view_)
      CreateAndOpenAppList();
    apps_grid_view_ = app_list_view_->app_list_main_view()
                          ->contents_view()
                          ->GetAppsContainerView()
                          ->apps_grid_view();
    apps_grid_test_api_ =
        std::make_unique<app_list::test::AppsGridViewTestApi>(apps_grid_view_);
  }
  gfx::Rect GetItemRectOnCurrentPageAt(int row, int col) const {
    DCHECK_GT(app_list_test_model_->top_level_item_list()->item_count(), 0u);
    return apps_grid_test_api_->GetItemTileRectOnCurrentPageAt(row, col);
  }

  app_list::test::AppListTestModel* app_list_test_model_ = nullptr;
  std::unique_ptr<app_list::test::AppsGridViewTestApi> apps_grid_test_api_;
  std::unique_ptr<app_list::test::AppListTestViewDelegate>
      app_list_test_delegate_;
  app_list::AppListView* app_list_view_ = nullptr;  // Owned by native widget.
  app_list::AppsGridView* apps_grid_view_ =
      nullptr;  // Owned by |app_list_view_|.
};

// Instantiate the Boolean which is used to toggle mouse and touch events in
// the parameterized tests.
INSTANTIATE_TEST_SUITE_P(, AppListPresenterDelegateTest, testing::Bool());

TEST_F(PopulatedAppListTest, TappingAppsGridClosesVirtualKeyboard) {
  InitializeAppsGrid();
  app_list_test_model_->PopulateApps(2);
  gfx::Point between_apps = GetItemRectOnCurrentPageAt(0, 0).right_center();
  gfx::Point empty_space = GetItemRectOnCurrentPageAt(0, 2).CenterPoint();

  ui::GestureEvent tap_between(between_apps.x(), between_apps.y(), 0,
                               base::TimeTicks(),
                               ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  ui::GestureEvent tap_outside(empty_space.x(), empty_space.y(), 0,
                               base::TimeTicks(),
                               ui::GestureEventDetails(ui::ET_GESTURE_TAP));

  // Manually show the virtual keyboard.
  auto* const keyboard_controller = keyboard::KeyboardController::Get();
  keyboard_controller->ShowKeyboard(true /* locked */);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Touch the apps_grid outside of any apps
  apps_grid_view_->OnGestureEvent(&tap_outside);
  // Expect that the event is ignored here and allowed to propogate to app_list
  EXPECT_FALSE(tap_outside.handled());
  // Hit the app_list with the same event
  app_list_view_->OnGestureEvent(&tap_outside);
  // Expect that the event is handled and the keyboard is closed.
  EXPECT_TRUE(tap_outside.handled());
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());

  // Reshow the VKeyboard
  keyboard_controller->ShowKeyboard(true);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Touch the apps_grid between two apps
  apps_grid_view_->OnGestureEvent(&tap_between);
  // Expect the event to be handled in the grid, and the keyboard to be closed.
  EXPECT_TRUE(tap_between.handled());
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());
}

// Tests that app list hides when focus moves to a normal window.
TEST_F(AppListPresenterDelegateTest, HideOnFocusOut) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window.get());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that app list remains visible when focus is moved to a different
// window in kShellWindowId_AppListContainer.
TEST_F(AppListPresenterDelegateTest,
       RemainVisibleWhenFocusingToApplistContainer) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  aura::Window* applist_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AppListContainer);
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(0, applist_container));
  wm::ActivateWindow(window.get());
  GetAppListTestHelper()->WaitUntilIdle();

  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests opening the app list on a secondary display, then deleting the display.
TEST_F(AppListPresenterDelegateTest, NonPrimaryDisplay) {
  // Set up a screen with two displays (horizontally adjacent).
  UpdateDisplay("1024x768,1024x768");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  ASSERT_EQ("1024,0 1024x768", root_windows[1]->GetBoundsInScreen().ToString());

  GetAppListTestHelper()->ShowAndRunLoop(GetSecondaryDisplay().id());
  GetAppListTestHelper()->CheckVisibility(true);

  // Remove the secondary display. Shouldn't crash (http://crbug.com/368990).
  UpdateDisplay("1024x768");

  // Updating the displays should close the app list.
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests updating display should not close the app list.
TEST_F(AppListPresenterDelegateTest, UpdateDisplayNotCloseAppList) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  // Change display bounds.
  UpdateDisplay("1024x768");

  // Updating the display should not close the app list.
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests the app list window's bounds under multi-displays environment.
TEST_F(AppListPresenterDelegateTest, AppListWindowBounds) {
  // Set up a screen with two displays (horizontally adjacent).
  UpdateDisplay("1024x768,1024x768");
  const gfx::Size display_size(1024, 768);

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  // Test the app list window's bounds on primary display.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect primary_display_rect(
      gfx::Point(
          0, display_size.height() -
                 app_list::AppListConfig::instance().peeking_app_list_height()),
      display_size);
  EXPECT_EQ(
      primary_display_rect,
      GetAppListView()->GetWidget()->GetNativeView()->GetBoundsInScreen());

  // Close the app list on primary display.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);

  // Test the app list window's bounds on secondary display.
  GetAppListTestHelper()->ShowAndRunLoop(GetSecondaryDisplay().id());
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect secondary_display_rect(
      gfx::Point(
          display_size.width(),
          display_size.height() -
              app_list::AppListConfig::instance().peeking_app_list_height()),
      display_size);
  EXPECT_EQ(
      secondary_display_rect,
      GetAppListView()->GetWidget()->GetNativeView()->GetBoundsInScreen());
}

// Tests that the app list is not draggable in side shelf alignment.
TEST_F(AppListPresenterDelegateTest, SideShelfAlignmentDragDisabled) {
  SetShelfAlignment(ShelfAlignment::SHELF_ALIGNMENT_RIGHT);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  const app_list::AppListView* app_list = GetAppListView();
  EXPECT_TRUE(app_list->is_fullscreen());
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenAllApps);

  // Drag the widget across the screen over an arbitrary 100Ms, this would
  // normally result in the app list transitioning to PEEKING but will now
  // result in no state change.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureScrollSequence(GetPointOutsideSearchbox(),
                                   gfx::Point(10, 900),
                                   base::TimeDelta::FromMilliseconds(100), 10);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenAllApps);

  // Tap the app list body. This should still close the app list.
  generator->GestureTapAt(GetPointOutsideSearchbox());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that the app list initializes in fullscreen with side shelf alignment
// and that the state transitions via text input act properly.
TEST_F(AppListPresenterDelegateTest, SideShelfAlignmentTextStateTransitions) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  SetShelfAlignment(ShelfAlignment::SHELF_ALIGNMENT_LEFT);

  // Open the app list with side shelf alignment, then check that it is in
  // fullscreen mode.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  app_list::AppListView* app_list = GetAppListView();
  EXPECT_TRUE(app_list->is_fullscreen());
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenAllApps);

  // Enter text in the searchbox, the app list should transition to fullscreen
  // search.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenSearch);

  // Delete the text in the searchbox, the app list should transition to
  // fullscreen all apps.
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenAllApps);
}

// Tests that the app list initializes in peeking with bottom shelf alignment
// and that the state transitions via text input act properly.
TEST_F(AppListPresenterDelegateTest, BottomShelfAlignmentTextStateTransitions) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  app_list::AppListView* app_list = GetAppListView();
  EXPECT_FALSE(app_list->is_fullscreen());
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);

  // Enter text in the searchbox, this should transition the app list to half
  // state.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kHalf);

  // Empty the searchbox, this should transition the app list to it's previous
  // state.
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);
}

// Tests that the app list initializes in fullscreen with tablet mode active
// and that the state transitions via text input act properly.
TEST_F(AppListPresenterDelegateTest, TabletModeTextStateTransitions) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  EnableTabletMode(true);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenAllApps);

  // Enter text in the searchbox, the app list should transition to fullscreen
  // search.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenSearch);

  // Delete the text in the searchbox, the app list should transition to
  // fullscreen all apps.
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenAllApps);
}

// Tests that the app list state responds correctly to tablet mode being
// enabled while the app list is being shown with half launcher.
TEST_F(AppListPresenterDelegateTest, HalfToFullscreenWhenTabletModeIsActive) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);

  // Enter text in the search box to transition to half app list.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kHalf);

  // Enable tablet mode and force the app list to transition to the fullscreen
  // equivalent of the current state.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenSearch);
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenAllApps);
}

// Tests that the app list view handles drag properly in laptop mode.
TEST_F(AppListPresenterDelegateTest, AppListViewDragHandler) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);

  ui::test::EventGenerator* generator = GetEventGenerator();
  // Execute a slow short upwards drag this should fail to transition the app
  // list.
  int top_of_app_list =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen().y();
  generator->GestureScrollSequence(gfx::Point(0, top_of_app_list + 20),
                                   gfx::Point(0, top_of_app_list - 20),
                                   base::TimeDelta::FromMilliseconds(500), 50);
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);

  // Execute a long upwards drag, this should transition the app list.
  generator->GestureScrollSequence(gfx::Point(10, top_of_app_list + 20),
                                   gfx::Point(10, 10),
                                   base::TimeDelta::FromMilliseconds(100), 10);
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenAllApps);

  // Execute a short downward drag, this should fail to transition the app list.
  gfx::Point start(10, 10);
  gfx::Point end(10, 100);
  generator->GestureScrollSequence(
      start, end,
      generator->CalculateScrollDurationForFlingVelocity(start, end, 1, 100),
      100);
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenAllApps);

  // Execute a long and slow downward drag to switch to peeking.
  start = gfx::Point(10, 200);
  end = gfx::Point(10, 650);
  generator->GestureScrollSequence(
      start, end,
      generator->CalculateScrollDurationForFlingVelocity(start, end, 1, 100),
      100);
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);

  // Transition to fullscreen.
  generator->GestureScrollSequence(gfx::Point(10, top_of_app_list + 20),
                                   gfx::Point(10, 10),
                                   base::TimeDelta::FromMilliseconds(100), 10);
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenAllApps);

  // Enter text to transition to |FULLSCREEN_SEARCH|.
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenSearch);

  // Execute a short downward drag, this should fail to close the app list.
  start = gfx::Point(10, 10);
  end = gfx::Point(10, 100);
  generator->GestureScrollSequence(
      start, end,
      generator->CalculateScrollDurationForFlingVelocity(start, end, 1, 100),
      100);
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenSearch);

  // Execute a long downward drag, this should close the app list.
  generator->GestureScrollSequence(gfx::Point(10, 10), gfx::Point(10, 900),
                                   base::TimeDelta::FromMilliseconds(100), 10);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that the bottom shelf background is hidden when the app list is shown
// in laptop mode.
TEST_F(AppListPresenterDelegateTest,
       ShelfBackgroundIsHiddenWhenAppListIsShown) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  ShelfLayoutManager* shelf_layout_manager =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()))
          ->shelf_layout_manager();
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_APP_LIST,
            shelf_layout_manager->GetShelfBackgroundType());
}

// Tests the shelf background type is as expected when in tablet mode.
TEST_F(AppListPresenterDelegateTest, ShelfBackgroundWithHomeLauncher) {
  // Enter tablet mode to display the home launcher.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EnableTabletMode(true);
  ShelfLayoutManager* shelf_layout_manager =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()))
          ->shelf_layout_manager();
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_DEFAULT,
            shelf_layout_manager->GetShelfBackgroundType());

  // Add a window. It should be maximized because it is in tablet mode.
  auto window = CreateTestWindow();
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_MAXIMIZED,
            shelf_layout_manager->GetShelfBackgroundType());
}

// Tests that the bottom shelf is auto hidden when a window is fullscreened in
// tablet mode (home launcher is shown behind).
TEST_F(AppListPresenterDelegateTest, ShelfAutoHiddenWhenFullscreen) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EnableTabletMode(true);
  Shelf* shelf =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()));
  EXPECT_EQ(ShelfVisibilityState::SHELF_VISIBLE, shelf->GetVisibilityState());

  // Create and fullscreen a window. The shelf should be auto hidden.
  auto window = CreateTestWindow();
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(ShelfVisibilityState::SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(ShelfAutoHideState::SHELF_AUTO_HIDE_HIDDEN,
            shelf->GetAutoHideState());
}

// Tests that the peeking app list closes if the user taps or clicks outside
// its bounds.
TEST_P(AppListPresenterDelegateTest, TapAndClickOutsideClosesPeekingAppList) {
  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Tapping outside the bounds closes the app list.
  const gfx::Rect peeking_bounds = GetAppListView()->GetBoundsInScreen();
  gfx::Point tap_point = peeking_bounds.origin();
  tap_point.Offset(10, -10);
  ASSERT_FALSE(peeking_bounds.Contains(tap_point));

  if (test_mouse_event) {
    generator->MoveMouseTo(tap_point);
    generator->ClickLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(tap_point);
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

TEST_P(AppListPresenterDelegateTest, LongPressOutsideCloseAppList) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  // |outside_point| is outside the bounds of app list.
  gfx::Point outside_point = GetAppListView()->bounds().origin();
  outside_point.Offset(0, -10);

  // Dispatch LONG_PRESS to ash::AppListPresenterDelegate.
  ui::TouchEvent long_press(
      ui::ET_GESTURE_LONG_PRESS, outside_point, base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH));
  GetEventGenerator()->Dispatch(&long_press);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

TEST_P(AppListPresenterDelegateTest, TwoFingerTapOutsideCloseAppList) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  // |outside_point| is outside the bounds of app list.
  gfx::Point outside_point = GetAppListView()->bounds().origin();
  outside_point.Offset(0, -10);

  // Dispatch TWO_FINGER_TAP to ash::AppListPresenterDelegate.
  ui::TouchEvent two_finger_tap(
      ui::ET_GESTURE_TWO_FINGER_TAP, outside_point, base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH));
  GetEventGenerator()->Dispatch(&two_finger_tap);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that a keypress activates the searchbox and that clearing the
// searchbox, the searchbox remains active.
TEST_F(AppListPresenterDelegateTest, KeyPressEnablesSearchBox) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  ui::test::EventGenerator* generator = GetEventGenerator();
  app_list::SearchBoxView* search_box_view =
      GetAppListView()->search_box_view();
  EXPECT_FALSE(search_box_view->is_search_box_active());

  // Press any key, the search box should be active.
  generator->PressKey(ui::VKEY_0, 0);
  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Delete the text, the search box should be inactive.
  search_box_view->ClearSearch();
  EXPECT_TRUE(search_box_view->is_search_box_active());
}

// Tests that a tap/click on the AppListView from half launcher returns the
// AppListView to Peeking, and that a tap/click on the AppListView from
// Peeking closes the app list.
TEST_P(AppListPresenterDelegateTest,
       StateTransitionsByTapAndClickingAppListBodyFromHalf) {
  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  app_list::AppListView* app_list_view = GetAppListView();
  app_list::SearchBoxView* search_box_view = app_list_view->search_box_view();
  ui::test::EventGenerator* generator = GetEventGenerator();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);

  // Press a key, the AppListView should transition to half.
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kHalf);
  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Tap outside the search box, the AppListView should transition to Peeking
  // and the search box should be inactive.
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->ClickLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);
  EXPECT_FALSE(search_box_view->is_search_box_active());

  // Tap outside the search box again, the AppListView should hide.
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->ClickLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that a tap/click on the AppListView from Fullscreen search returns
// the AppListView to fullscreen all apps, and that a tap/click on the
// AppListView from fullscreen all apps closes the app list.
TEST_P(AppListPresenterDelegateTest,
       StateTransitionsByTappingAppListBodyFromFullscreen) {
  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  app_list::AppListView* app_list_view = GetAppListView();
  app_list::SearchBoxView* search_box_view = app_list_view->search_box_view();
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Execute a long upwards drag, this should transition the app list to
  // fullscreen.
  const int top_of_app_list =
      app_list_view->GetWidget()->GetWindowBoundsInScreen().y();
  generator->GestureScrollSequence(gfx::Point(10, top_of_app_list + 20),
                                   gfx::Point(10, 10),
                                   base::TimeDelta::FromMilliseconds(100), 10);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenAllApps);

  // Press a key, this should activate the searchbox and transition to
  // fullscreen search.
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenSearch);
  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Tap outside the searchbox, this should deactivate the searchbox and the
  // applistview should return to fullscreen all apps.
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->ClickLeftButton();
  } else {
    generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenAllApps);
  EXPECT_FALSE(search_box_view->is_search_box_active());

  // Tap outside the searchbox again, this should close the applistview.
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->ClickLeftButton();
  } else {
    generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that the searchbox activates when it is tapped and that the widget is
// closed after tapping outside the searchbox.
TEST_P(AppListPresenterDelegateTest, TapAndClickEnablesSearchBox) {
  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  app_list::SearchBoxView* search_box_view =
      GetAppListView()->search_box_view();

  // Tap/Click the search box, it should activate.
  ui::test::EventGenerator* generator = GetEventGenerator();
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointInsideSearchbox());
    generator->PressLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(GetPointInsideSearchbox());
  }

  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Tap on the body of the app list, the search box should deactivate.
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->PressLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_FALSE(search_box_view->is_search_box_active());
  GetAppListTestHelper()->CheckVisibility(true);

  // Tap on the body of the app list again, the app list should hide.
  if (test_mouse_event) {
    generator->PressLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that the shelf background displays/hides with bottom shelf
// alignment.
TEST_F(AppListPresenterDelegateTest,
       ShelfBackgroundRespondsToAppListBeingShown) {
  GetPrimaryShelf()->SetAlignment(SHELF_ALIGNMENT_BOTTOM);

  // Show the app list, the shelf background should be transparent.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  ShelfLayoutManager* shelf_layout_manager =
      GetPrimaryShelf()->shelf_layout_manager();
  EXPECT_EQ(SHELF_BACKGROUND_APP_LIST,
            shelf_layout_manager->GetShelfBackgroundType());
  GetAppListTestHelper()->DismissAndRunLoop();

  // Set the alignment to the side and show the app list. The background
  // should show.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::SHELF_ALIGNMENT_LEFT);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_FALSE(GetPrimaryShelf()->IsHorizontalAlignment());
  EXPECT_EQ(
      SHELF_BACKGROUND_APP_LIST,
      GetPrimaryShelf()->shelf_layout_manager()->GetShelfBackgroundType());
}

// Tests that the half app list closes itself if the user taps outside its
// bounds.
TEST_P(AppListPresenterDelegateTest, TapAndClickOutsideClosesHalfAppList) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Transition to half app list by entering text.
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kHalf);

  // A point outside the bounds of launcher.
  gfx::Point to_point(
      0, GetAppListView()->GetWidget()->GetWindowBoundsInScreen().y() - 1);

  // Clicking/tapping outside the bounds closes the app list.
  if (TestMouseEventParam()) {
    generator->MoveMouseTo(to_point);
    generator->ClickLeftButton();
  } else {
    generator->GestureTapAt(to_point);
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that the search box is set active with a whitespace query and that the
// app list state doesn't transition with a whitespace query.
TEST_F(AppListPresenterDelegateTest, WhitespaceQuery) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  app_list::AppListView* view = GetAppListView();
  ui::test::EventGenerator* generator = GetEventGenerator();
  EXPECT_FALSE(view->search_box_view()->is_search_box_active());
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);

  // Enter a whitespace query, the searchbox should activate but stay in peeking
  // mode.
  generator->PressKey(ui::VKEY_SPACE, 0);
  EXPECT_TRUE(view->search_box_view()->is_search_box_active());
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);

  // Enter a non-whitespace character, the Searchbox should stay active and go
  // to HALF
  generator->PressKey(ui::VKEY_0, 0);
  EXPECT_TRUE(view->search_box_view()->is_search_box_active());
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kHalf);

  // Delete the non whitespace character, the Searchbox should not deactivate
  // but go to PEEKING
  generator->PressKey(ui::VKEY_BACK, 0);
  EXPECT_TRUE(view->search_box_view()->is_search_box_active());
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);
}

// Tests that an unhandled two finger tap/right click does not close the app
// list, and an unhandled one finger tap/left click closes the app list in
// Peeking mode.
TEST_P(AppListPresenterDelegateTest, UnhandledEventOnPeeking) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);

  // Two finger tap or right click in the empty space below the searchbox. The
  // app list should not close.
  gfx::Point empty_space =
      GetAppListView()->search_box_view()->GetBoundsInScreen().bottom_left();
  empty_space.Offset(0, 10);
  ui::test::EventGenerator* generator = GetEventGenerator();
  if (TestMouseEventParam()) {
    generator->MoveMouseTo(empty_space);
    generator->PressRightButton();
    generator->ReleaseRightButton();
  } else {
    ui::GestureEvent two_finger_tap(
        empty_space.x(), empty_space.y(), 0, base::TimeTicks(),
        ui::GestureEventDetails(ui::ET_GESTURE_TWO_FINGER_TAP));
    generator->Dispatch(&two_finger_tap);
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);
  GetAppListTestHelper()->CheckVisibility(true);

  // One finger tap or left click in the empty space below the searchbox. The
  // app list should close.
  if (TestMouseEventParam()) {
    generator->MoveMouseTo(empty_space);
    generator->ClickLeftButton();
  } else {
    generator->GestureTapAt(empty_space);
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that a drag to the bezel from Fullscreen/Peeking will close the app
// list.
TEST_P(AppListPresenterDelegateTest,
       DragToBezelClosesAppListFromFullscreenAndPeeking) {
  const bool test_fullscreen = GetParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  app_list::AppListView* view = GetAppListView();
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);

  if (test_fullscreen) {
    FlingUpOrDown(GetEventGenerator(), view, true /* up */);
    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckState(
        ash::mojom::AppListViewState::kFullscreenAllApps);
  }

  // Drag the app list to 50 DIPs from the bottom bezel.
  ui::test::EventGenerator* generator = GetEventGenerator();
  const int bezel_y = display::Screen::GetScreen()
                          ->GetDisplayNearestView(view->parent_window())
                          .bounds()
                          .bottom();
  generator->GestureScrollSequence(
      gfx::Point(0, bezel_y - (kAppListBezelMargin + 100)),
      gfx::Point(0, bezel_y - (kAppListBezelMargin)),
      base::TimeDelta::FromMilliseconds(1500), 100);

  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that a fling from Fullscreen/Peeking closes the app list.
TEST_P(AppListPresenterDelegateTest,
       FlingDownClosesAppListFromFullscreenAndPeeking) {
  const bool test_fullscreen = GetParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  app_list::AppListView* view = GetAppListView();
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);

  if (test_fullscreen) {
    FlingUpOrDown(GetEventGenerator(), view, true /* up */);
    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckState(
        ash::mojom::AppListViewState::kFullscreenAllApps);
  }

  // Fling down, the app list should close.
  FlingUpOrDown(GetEventGenerator(), view, false /* down */);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

TEST_F(AppListPresenterDelegateTest,
       MouseWheelFromAppListPresenterImplTransitionsAppListState) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);

  GetAppListView()->HandleScroll(gfx::Vector2d(0, -30), ui::ET_MOUSEWHEEL);

  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenAllApps);
}

TEST_P(AppListPresenterDelegateTest, LongUpwardDragInFullscreenShouldNotClose) {
  const bool test_fullscreen_search = GetParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  app_list::AppListView* view = GetAppListView();
  FlingUpOrDown(GetEventGenerator(), view, true);
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenAllApps);

  if (test_fullscreen_search) {
    // Enter a character into the searchbox to transition to FULLSCREEN_SEARCH.
    GetEventGenerator()->PressKey(ui::VKEY_0, 0);
    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckState(
        ash::mojom::AppListViewState::kFullscreenSearch);
  }

  // Drag from the center of the applist to the top of the screen very slowly.
  // This should not trigger a state transition.
  gfx::Point drag_start = view->GetBoundsInScreen().CenterPoint();
  drag_start.set_x(15);
  gfx::Point drag_end = view->GetBoundsInScreen().top_right();
  drag_end.set_x(15);
  GetEventGenerator()->GestureScrollSequence(
      drag_start, drag_end,
      GetEventGenerator()->CalculateScrollDurationForFlingVelocity(
          drag_start, drag_end, 1, 1000),
      1000);
  GetAppListTestHelper()->WaitUntilIdle();
  if (test_fullscreen_search)
    GetAppListTestHelper()->CheckState(
        ash::mojom::AppListViewState::kFullscreenSearch);
  else
    GetAppListTestHelper()->CheckState(
        ash::mojom::AppListViewState::kFullscreenAllApps);
}

// Tests that a drag can not make the app list smaller than the shelf height.
TEST_F(AppListPresenterDelegateTest, LauncherCannotGetSmallerThanShelf) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  app_list::AppListView* view = GetAppListView();

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

// Tests that the AppListView is on screen on a small display.
TEST_F(AppListPresenterDelegateTest, SearchBoxShownOnSmallDisplay) {
  // Update the display to a small scale factor.
  UpdateDisplay("600x400");
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  // Animate to Half.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  app_list::AppListView* view = GetAppListView();
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kHalf);
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kHalf);
  EXPECT_LE(0, view->GetWidget()->GetNativeView()->bounds().y());

  // Animate to peeking.
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);
  EXPECT_LE(0, view->GetWidget()->GetNativeView()->bounds().y());

  // Animate back to Half.
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kHalf);
  EXPECT_LE(0, view->GetWidget()->GetNativeView()->bounds().y());
}

// Tests that the AppListView is on screen on a small work area.
TEST_F(AppListPresenterDelegateTest, SearchBoxShownOnSmallWorkArea) {
  // Update the work area to a small size.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  ASSERT_TRUE(display_manager()->UpdateWorkAreaOfDisplay(
      GetPrimaryDisplayId(), gfx::Insets(400, 0, 0, 0)));

  // Animate to Half.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  app_list::AppListView* view = GetAppListView();
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kHalf);
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kHalf);
  EXPECT_LE(GetPrimaryDisplay().work_area().y(),
            view->GetWidget()->GetNativeView()->bounds().y());

  // Animate to peeking.
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);
  EXPECT_LE(GetPrimaryDisplay().work_area().y(),
            view->GetWidget()->GetNativeView()->bounds().y());

  // Animate back to Half.
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kHalf);
  EXPECT_LE(GetPrimaryDisplay().work_area().y(),
            view->GetWidget()->GetNativeView()->bounds().y());
}

// Tests that no crash occurs after an attempt to show app list in an invalid
// display.
TEST_F(AppListPresenterDelegateTest, ShowInInvalidDisplay) {
  GetAppListTestHelper()->ShowAndRunLoop(display::kInvalidDisplayId);
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kClosed);
}

// Tests that tap the auto-hide shelf with app list opened should dismiss the
// app list but keep shelf visible.
TEST_F(AppListPresenterDelegateTest, TapAutoHideShelfWithAppListOpened) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  // Create a normal unmaximized window; the shelf should be hidden.
  std::unique_ptr<views::Widget> window = CreateTestWidget();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Tap the system tray should open system tray bubble and keep shelf visible.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureTapAt(
      GetPrimaryUnifiedSystemTray()->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Tap to dismiss the app list and the auto-hide shelf.
  generator->GestureTapAt(gfx::Point(0, 0));
  EXPECT_FALSE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  GetAppListTestHelper()->WaitUntilIdle();

  // Tap the auto-hide shelf area with app list opened should keep both app list
  // and shelf visible.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  generator->GestureTapAt(
      shelf->GetShelfViewForTesting()->GetBoundsInScreen().CenterPoint());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
}

// Test a variety of behaviors for home launcher (app list in tablet mode).
class AppListPresenterDelegateHomeLauncherTest
    : public AppListPresenterDelegateTest {
 public:
  AppListPresenterDelegateHomeLauncherTest() = default;
  ~AppListPresenterDelegateHomeLauncherTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {app_list_features::kEnableBackgroundBlur}, {});
    AppListPresenterDelegateTest::SetUp();
    GetAppListTestHelper()->WaitUntilIdle();
  }

  void PressAppListButton() {
    std::unique_ptr<ui::Event> event = std::make_unique<ui::MouseEvent>(
        ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), base::TimeTicks(),
        ui::EF_NONE, 0);
    Shell::Get()
        ->shelf_controller()
        ->model()
        ->GetShelfItemDelegate(ShelfID(kAppListId))
        ->ItemSelected(std::move(event), GetPrimaryDisplayId(),
                       ash::LAUNCH_FROM_UNKNOWN, base::DoNothing());
    GetAppListTestHelper()->WaitUntilIdle();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterDelegateHomeLauncherTest);
};

// Tests that the app list is shown automatically when the tablet mode is on.
// The app list is dismissed when the tablet mode is off.
TEST_F(AppListPresenterDelegateHomeLauncherTest, ShowAppListForTabletMode) {
  GetAppListTestHelper()->CheckVisibility(false);

  // Turns on tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  // Turns off tablet mode.
  EnableTabletMode(false);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that the app list window's parent is changed after entering tablet
// mode.
TEST_F(AppListPresenterDelegateHomeLauncherTest, ParentWindowContainer) {
  // Show app list in non-tablet mode. The window container should be
  // kShellWindowId_AppListContainer.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  aura::Window* window = GetAppListView()->GetWidget()->GetNativeWindow();
  aura::Window* root_window = window->GetRootWindow();
  EXPECT_TRUE(root_window->GetChildById(kShellWindowId_AppListContainer)
                  ->Contains(window));

  // Turn on tablet mode. The window container should be
  // kShellWindowId_HomeScreenContainer.
  EnableTabletMode(true);
  EXPECT_TRUE(root_window->GetChildById(kShellWindowId_HomeScreenContainer)
                  ->Contains(window));
}

// Tests that the background opacity change for app list.
TEST_F(AppListPresenterDelegateHomeLauncherTest, BackgroundOpacity) {
  // Show app list in non-tablet mode. The background sheild opacity should be
  // 70%.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  // The opacity should be set on the color, not the layer. Setting opacity on
  // the layer will change the opacity of the blur effect, which is not desired.
  const U8CPU clamshell_background_opacity = static_cast<U8CPU>(255 * 0.74);
  EXPECT_EQ(SkColorSetA(app_list::AppListView::kDefaultBackgroundColor,
                        clamshell_background_opacity),
            GetAppListView()->GetAppListBackgroundShieldColorForTest());
  const ui::Layer* background_layer =
      GetAppListView()->GetAppListBackgroundShieldForTest()->layer();
  EXPECT_EQ(1, background_layer->opacity());

  // Turn on tablet mode. The background shield should be transparent.
  EnableTabletMode(true);

  const U8CPU tablet_background_opacity = static_cast<U8CPU>(0);
  EXPECT_EQ(SkColorSetA(app_list::AppListView::kDefaultBackgroundColor,
                        tablet_background_opacity),
            GetAppListView()->GetAppListBackgroundShieldColorForTest());
  EXPECT_EQ(1, background_layer->opacity());
}

// Tests that the background blur which is present in clamshell mode does not
// show in tablet mode.
TEST_F(AppListPresenterDelegateHomeLauncherTest, BackgroundBlur) {
  // Show app list in non-tablet mode. The background blur should be enabled.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  ui::Layer* background_layer =
      GetAppListView()->GetAppListBackgroundShieldForTest()->layer();
  EXPECT_GT(background_layer->background_blur(), 0.0f);

  // Turn on tablet mode. The background blur should be disabled.
  EnableTabletMode(true);
  EXPECT_EQ(0.0f, background_layer->background_blur());
}

// Tests that tapping or clicking on background cannot dismiss the app list.
TEST_F(AppListPresenterDelegateHomeLauncherTest, TapOrClickToDismiss) {
  // Show app list in non-tablet mode. Click outside search box.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(GetPointOutsideSearchbox());
  generator->PressLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Show app list in non-tablet mode. Tap outside search box.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Show app list in tablet mode. Click outside search box.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  generator->MoveMouseTo(GetPointOutsideSearchbox());
  generator->PressLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);

  // Tap outside search box.
  generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that accelerator Escape, Broswer back and Search key cannot dismiss the
// appt list.
TEST_F(AppListPresenterDelegateHomeLauncherTest, PressAcceleratorToDismiss) {
  // Show app list in non-tablet mode. Press Escape key.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_ESCAPE, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Show app list in non-tablet mode. Press Browser back key.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  generator->PressKey(ui::KeyboardCode::VKEY_BROWSER_BACK, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Show app list in non-tablet mode. Press search key.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  generator->PressKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Show app list in tablet mode. Press Escape key.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  generator->PressKey(ui::KeyboardCode::VKEY_ESCAPE, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);

  // Press Browser back key.
  generator->PressKey(ui::KeyboardCode::VKEY_BROWSER_BACK, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);

  // Press search key.
  generator->PressKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that moving focus outside app list window cannot dismiss it.
TEST_F(AppListPresenterDelegateHomeLauncherTest, FocusOutToDismiss) {
  // Show app list in non-tablet mode. Move focus to another window.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window.get());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Show app list in tablet mode. Move focus to another window.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  wm::ActivateWindow(window.get());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that the gesture-scroll cannot dismiss the app list.
TEST_F(AppListPresenterDelegateHomeLauncherTest, GestureScrollToDismiss) {
  // Show app list in non-tablet mode. Fling down.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  FlingUpOrDown(GetEventGenerator(), GetAppListView(), false /* up */);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Show app list in tablet mode. Fling down.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  FlingUpOrDown(GetEventGenerator(), GetAppListView(), false /* up */);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that the mouse-scroll cannot dismiss the app list.
TEST_F(AppListPresenterDelegateHomeLauncherTest,
       MouseScrollDoesntDismissPeekingLauncher) {
  // Show app list in non-tablet mode. Mouse-scroll up.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);
  GetAppListTestHelper()->CheckVisibility(true);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(GetPointOutsideSearchbox());

  // Scroll up to get fullscreen.
  generator->MoveMouseWheel(0, 1);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenAllApps);
  GetAppListTestHelper()->CheckVisibility(true);

  // Reset and show app list in non-tablet mode. Mouse-scroll down.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);
  GetAppListTestHelper()->CheckVisibility(true);

  // Scroll down to get fullscreen.
  generator->MoveMouseWheel(0, -1);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenAllApps);
  GetAppListTestHelper()->CheckVisibility(true);

  // Show app list in tablet mode. Mouse-scroll up.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  generator->MoveMouseTo(GetPointOutsideSearchbox());
  generator->MoveMouseWheel(0, 1);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that mouse-scroll up at fullscreen will dismiss app list.
TEST_F(AppListPresenterDelegateHomeLauncherTest,
       MouseScrollToDismissFromFullscreen) {
  // Show app list in non-tablet mode. Mouse-scroll down.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kPeeking);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(GetPointOutsideSearchbox());

  // Scroll up with mouse wheel to fullscreen.
  generator->MoveMouseWheel(0, 1);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenAllApps);
  GetAppListTestHelper()->CheckVisibility(true);
  generator->MoveMouseTo(GetPointOutsideSearchbox());

  // Scroll up with mouse wheel to close app list.
  generator->MoveMouseWheel(0, 1);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::mojom::AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests the app list opacity in overview mode.
TEST_F(AppListPresenterDelegateHomeLauncherTest, OpacityInOverviewMode) {
  // Show app list in tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  // Enable overview mode.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->IsSelecting());
  ui::Layer* layer = GetAppListView()->GetWidget()->GetNativeWindow()->layer();
  EXPECT_EQ(0.0f, layer->opacity());

  // Disable overview mode.
  overview_controller->ToggleOverview();
  EXPECT_FALSE(overview_controller->IsSelecting());
  EXPECT_EQ(1.0f, layer->opacity());
}

// Tests the app list visibility during wallpaper preview.
TEST_F(AppListPresenterDelegateHomeLauncherTest,
       VisibilityDuringWallpaperPreview) {
  WallpaperControllerTestApi wallpaper_test_api(
      Shell::Get()->wallpaper_controller());
  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));

  // The app list is hidden in the beginning.
  GetAppListTestHelper()->CheckVisibility(false);
  // Open wallpaper picker and start preview. Verify the app list remains
  // hidden.
  wm::GetWindowState(wallpaper_picker_window.get())->Activate();
  wallpaper_test_api.StartWallpaperPreview();
  GetAppListTestHelper()->CheckVisibility(false);
  // Enable tablet mode. Verify the app list is still hidden because wallpaper
  // preview is active.
  EnableTabletMode(true);
  EXPECT_FALSE(GetAppListView()->GetWidget()->IsVisible());
  // End preview by confirming the wallpaper. Verify the app list is shown.
  wallpaper_test_api.EndWallpaperPreview(true /*confirm_preview_wallpaper=*/);
  GetAppListTestHelper()->CheckVisibility(true);

  // Start preview again. Verify the app list is hidden.
  wallpaper_test_api.StartWallpaperPreview();
  EXPECT_FALSE(GetAppListView()->GetWidget()->IsVisible());
  // End preview by canceling the wallpaper. Verify the app list is shown.
  wallpaper_test_api.EndWallpaperPreview(false /*confirm_preview_wallpaper=*/);
  GetAppListTestHelper()->CheckVisibility(true);

  // Start preview again and enable overview mode during the wallpaper preview.
  // Verify the app list is hidden.
  wallpaper_test_api.StartWallpaperPreview();
  EXPECT_FALSE(GetAppListView()->GetWidget()->IsVisible());
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->IsSelecting());
  EXPECT_FALSE(GetAppListView()->GetWidget()->IsVisible());
  // Disable overview mode. Verify the app list is still hidden because
  // wallpaper preview is still active.
  overview_controller->ToggleOverview();
  EXPECT_FALSE(overview_controller->IsSelecting());
  EXPECT_FALSE(GetAppListView()->GetWidget()->IsVisible());
  // End preview by confirming the wallpaper. Verify the app list is shown.
  wallpaper_test_api.EndWallpaperPreview(true /*confirm_preview_wallpaper=*/);
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that the app list button will minimize all windows.
TEST_F(AppListPresenterDelegateHomeLauncherTest,
       AppListButtonMinimizeAllWindows) {
  // Show app list in tablet mode. Maximize all windows.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0)),
      window2(CreateTestWindowInShellWithId(1)),
      window3(CreateTestWindowInShellWithId(2));
  wm::WindowState *state1 = wm::GetWindowState(window1.get()),
                  *state2 = wm::GetWindowState(window2.get()),
                  *state3 = wm::GetWindowState(window3.get());
  state1->Maximize();
  state2->Maximize();
  state3->Maximize();
  EXPECT_TRUE(state1->IsMaximized());
  EXPECT_TRUE(state2->IsMaximized());
  EXPECT_TRUE(state3->IsMaximized());

  // The windows need to be activated for the mru window tracker.
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window3.get());
  auto ordering = Shell::Get()->mru_window_tracker()->BuildWindowForCycleList();

  // Press app list button.
  PressAppListButton();
  EXPECT_TRUE(state1->IsMinimized());
  EXPECT_TRUE(state2->IsMinimized());
  EXPECT_TRUE(state3->IsMinimized());
  GetAppListTestHelper()->CheckVisibility(true);

  // Tests that the window ordering remains the same as before we minimize.
  EXPECT_TRUE(std::equal(
      ordering.begin(), ordering.end(),
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList().begin()));
}

// Tests that the app list button will end split view mode.
TEST_F(AppListPresenterDelegateHomeLauncherTest,
       AppListButtonEndSplitViewMode) {
  // Show app list in tablet mode. Enter split view mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  split_view_controller->SnapWindow(window.get(), SplitViewController::LEFT);
  EXPECT_TRUE(split_view_controller->IsSplitViewModeActive());

  // Press app list button.
  PressAppListButton();
  EXPECT_FALSE(split_view_controller->IsSplitViewModeActive());
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that the app list button will end overview mode.
TEST_F(AppListPresenterDelegateHomeLauncherTest, AppListButtonEndOverviewMode) {
  // Show app list in tablet mode. Enter overview mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->IsSelecting());

  // Press app list button.
  PressAppListButton();
  EXPECT_FALSE(overview_controller->IsSelecting());
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that the context menu is triggered in the same way as if we are on
// the wallpaper.
TEST_F(AppListPresenterDelegateHomeLauncherTest, WallpaperContextMenu) {
  // Show app list in tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  // Long press on the app list to open the context menu.
  // TODO(ginko) look into a way to populate an apps grid, then get a point
  // between these apps so that clicks/taps between apps can be tested
  const gfx::Point onscreen_point(GetPointOutsideSearchbox());
  ui::test::EventGenerator* generator = GetEventGenerator();
  ui::GestureEvent long_press(
      onscreen_point.x(), onscreen_point.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  generator->Dispatch(&long_press);
  GetAppListTestHelper()->WaitUntilIdle();
  const aura::Window* root = wm::GetRootWindowAt(onscreen_point);
  const RootWindowController* root_window_controller =
      RootWindowController::ForWindow(root);
  EXPECT_TRUE(root_window_controller->IsContextMenuShown());

  // Tap down to close the context menu.
  ui::GestureEvent tap_down(onscreen_point.x(), onscreen_point.y(), 0,
                            base::TimeTicks(),
                            ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));
  generator->Dispatch(&tap_down);
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_FALSE(root_window_controller->IsContextMenuShown());

  // Right click to open the context menu.
  generator->MoveMouseTo(onscreen_point);
  generator->PressRightButton();
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_TRUE(root_window_controller->IsContextMenuShown());

  // Left click to close the context menu.
  generator->MoveMouseTo(onscreen_point);
  generator->PressLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_FALSE(root_window_controller->IsContextMenuShown());
}

// Tests app list visibility when switching to tablet mode during dragging from
// shelf.
TEST_F(AppListPresenterDelegateHomeLauncherTest,
       SwitchToTabletModeDuringDraggingFromShelf) {
  UpdateDisplay("1080x900");
  GetAppListTestHelper()->CheckVisibility(false);

  // Drag from the shelf to show the app list.
  ui::test::EventGenerator* generator = GetEventGenerator();
  const int x = 540;
  const int closed_y = 890;
  const int fullscreen_y = 0;
  generator->MoveTouch(gfx::Point(x, closed_y));
  generator->PressTouch();
  generator->MoveTouch(gfx::Point(x, fullscreen_y));
  GetAppListTestHelper()->CheckVisibility(true);

  // Drag to shelf to close app list.
  generator->MoveTouch(gfx::Point(x, closed_y));
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Drag from the shelf to show the app list.
  generator->MoveTouch(gfx::Point(x, closed_y));
  generator->PressTouch();
  generator->MoveTouch(gfx::Point(x, fullscreen_y));
  GetAppListTestHelper()->CheckVisibility(true);

  // Switch to tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  // Drag to shelf to try to close app list.
  generator->MoveTouch(gfx::Point(x, closed_y));
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests app list visibility when switching to tablet mode during dragging to
// close app list.
TEST_F(AppListPresenterDelegateHomeLauncherTest,
       SwitchToTabletModeDuringDraggingToClose) {
  UpdateDisplay("1080x900");

  // Open app list.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  // Drag to shelf to close app list.
  ui::test::EventGenerator* generator = GetEventGenerator();
  const int x = 540;
  const int peeking_height =
      900 - app_list::AppListConfig::instance().peeking_app_list_height();
  const int closed_y = 890;
  generator->MoveTouch(gfx::Point(x, peeking_height));
  generator->PressTouch();
  generator->MoveTouch(gfx::Point(x, closed_y));
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Open app list.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  // Drag to shelf to close app list, meanwhile switch to tablet mode.
  generator->MoveTouch(gfx::Point(x, peeking_height));
  generator->PressTouch();
  generator->MoveTouch(gfx::Point(x, peeking_height + 10));
  EnableTabletMode(true);
  generator->MoveTouch(gfx::Point(x, closed_y));
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Test backdrop exists for active non-fullscreen window in tablet mode.
TEST_F(AppListPresenterDelegateHomeLauncherTest, BackdropTest) {
  WorkspaceControllerTestApi test_helper(
      ShellTestApi(Shell::Get()).workspace_controller());
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_FALSE(test_helper.GetBackdropWindow());

  std::unique_ptr<aura::Window> non_fullscreen_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  non_fullscreen_window->Show();
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_TRUE(test_helper.GetBackdropWindow());
}

// Tests that app list is not active when switching to tablet mode if an active
// window exists.
TEST_F(AppListPresenterDelegateHomeLauncherTest,
       NotActivateAppListWindowWhenActiveWindowExists) {
  // No window is active.
  EXPECT_EQ(nullptr, wm::GetActiveWindow());

  // Show app list in tablet mode. It should become active.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(GetAppListView()->GetWidget()->GetNativeWindow(),
            wm::GetActiveWindow());

  // End tablet mode.
  EnableTabletMode(false);
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(nullptr, wm::GetActiveWindow());

  // Activate a window.
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  wm::GetWindowState(window.get())->Activate();
  EXPECT_EQ(window.get(), wm::GetActiveWindow());

  // Show app list in tablet mode. It should not be active.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(window.get(), wm::GetActiveWindow());
}

// Tests that involve the virtual keyboard.
class AppListPresenterDelegateVirtualKeyboardTest
    : public AppListPresenterDelegateTest {
 public:
  AppListPresenterDelegateVirtualKeyboardTest() = default;
  ~AppListPresenterDelegateVirtualKeyboardTest() override = default;

  // AppListPresenterDelegateTest:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AppListPresenterDelegateTest::SetUp();
  }
};

// Instantiate the Boolean which is used to toggle mouse and touch events in
// the parameterized tests.
INSTANTIATE_TEST_SUITE_P(,
                         AppListPresenterDelegateVirtualKeyboardTest,
                         testing::Bool());

// Tests that tapping or clicking the body of the applist with an active virtual
// keyboard results in the virtual keyboard closing with no side effects.
TEST_P(AppListPresenterDelegateVirtualKeyboardTest,
       TapAppListWithVirtualKeyboardDismissesVirtualKeyboard) {
  const bool test_click = GetParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EnableTabletMode(true);

  // Tap to activate the searchbox.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureTapAt(GetPointInsideSearchbox());

  // Enter some text in the searchbox, the applist should transition to
  // fullscreen search.
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenSearch);

  // Manually show the virtual keyboard.
  auto* const keyboard_controller = keyboard::KeyboardController::Get();
  keyboard_controller->ShowKeyboard(true);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Tap or click outside the searchbox, the virtual keyboard should hide.
  if (test_click) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->ClickLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(GetPointOutsideSearchbox());
  }
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());

  // The searchbox should still be active and the AppListView should still be in
  // FULLSCREEN_SEARCH.
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenSearch);
  EXPECT_TRUE(GetAppListView()->search_box_view()->is_search_box_active());

  // Tap or click the body of the AppList again, the searchbox should deactivate
  // and the applist should be in FULLSCREEN_ALL_APPS.
  if (test_click) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->ClickLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->CheckState(
      ash::mojom::AppListViewState::kFullscreenAllApps);
  EXPECT_FALSE(GetAppListView()->search_box_view()->is_search_box_active());
}

}  // namespace ash
