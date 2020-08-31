// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/drag_window_from_shelf_controller.h"

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/home_screen/drag_window_from_shelf_controller_test_api.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/home_screen/home_screen_delegate.h"
#include "ash/public/cpp/overview_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_metrics.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Helper function to get the index of |child|, given its parent window
// |parent|.
int IndexOf(aura::Window* child, aura::Window* parent) {
  aura::Window::Windows children = parent->children();
  auto it = std::find(children.begin(), children.end(), child);
  DCHECK(it != children.end());

  return static_cast<int>(std::distance(children.begin(), it));
}

}  // namespace

class DragWindowFromShelfControllerTest : public AshTestBase {
 public:
  DragWindowFromShelfControllerTest() = default;
  ~DragWindowFromShelfControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    TabletModeControllerTestApi().EnterTabletMode();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    // Destroy |window_drag_controller_| so that its scheduled task won't get
    // run after the test environment is gone.
    window_drag_controller_.reset();
    AshTestBase::TearDown();
  }

  void StartDrag(aura::Window* window,
                 const gfx::Point& location_in_screen,
                 HotseatState hotseat_state) {
    window_drag_controller_ = std::make_unique<DragWindowFromShelfController>(
        window, gfx::PointF(location_in_screen), hotseat_state);
  }
  void Drag(const gfx::Point& location_in_screen,
            float scroll_x,
            float scroll_y) {
    window_drag_controller_->Drag(gfx::PointF(location_in_screen), scroll_x,
                                  scroll_y);
  }
  void EndDrag(const gfx::Point& location_in_screen,
               base::Optional<float> velocity_y) {
    window_drag_controller_->EndDrag(gfx::PointF(location_in_screen),
                                     velocity_y);
    window_drag_controller_->FinalizeDraggedWindow();
  }
  void CancelDrag() { window_drag_controller_->CancelDrag(); }
  void WaitForHomeLauncherAnimationToFinish() {
    // Wait until home launcher animation finishes.
    while (GetAppListTestHelper()
               ->GetAppListView()
               ->GetWidget()
               ->GetLayer()
               ->GetAnimator()
               ->is_animating()) {
      base::RunLoop run_loop;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(),
          base::TimeDelta::FromMilliseconds(200));
      run_loop.Run();
    }
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

  DragWindowFromShelfController* window_drag_controller() {
    return window_drag_controller_.get();
  }

 private:
  std::unique_ptr<DragWindowFromShelfController> window_drag_controller_;

  DISALLOW_COPY_AND_ASSIGN(DragWindowFromShelfControllerTest);
};

// Tests that we may hide different sets of windows in different scenarios.
TEST_F(DragWindowFromShelfControllerTest, HideWindowDuringWindowDragging) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();

  auto window3 = CreateTestWindow();
  auto window2 = CreateTestWindow();
  auto window1 = CreateTestWindow();
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());

  StartDrag(window1.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 1.f, 1.f);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
  EndDrag(shelf_bounds.CenterPoint(), /*velocity_y=*/base::nullopt);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());

  // In splitview mode, the snapped windows will stay visible during dragging.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  StartDrag(window1.get(), shelf_bounds.left_center(), HotseatState::kExtended);
  Drag(gfx::Point(0, 200), 1.f, 1.f);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
  EndDrag(shelf_bounds.bottom_left(), /*velocity_y=*/base::nullopt);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());

  StartDrag(window2.get(), shelf_bounds.right_center(),
            HotseatState::kExtended);
  Drag(gfx::Point(400, 200), 1.f, 1.f);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
  EndDrag(shelf_bounds.bottom_right(), /*velocity_y=*/base::nullopt);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
}

// Test home launcher is hidden during dragging.
TEST_F(DragWindowFromShelfControllerTest, HideHomeLauncherDuringDraggingTest) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateTestWindow();
  StartDrag(window.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  Drag(gfx::Point(0, 200), 0.f, 1.f);
  aura::Window* home_screen_window =
      Shell::Get()->home_screen_controller()->delegate()->GetHomeScreenWindow();
  EXPECT_TRUE(home_screen_window);
  EXPECT_FALSE(home_screen_window->IsVisible());

  EndDrag(shelf_bounds.CenterPoint(),
          /*velocity_y=*/base::nullopt);
  EXPECT_TRUE(home_screen_window->IsVisible());
}

// Test the windows that were hidden before drag started may or may not reshow,
// depending on different scenarios.
TEST_F(DragWindowFromShelfControllerTest, MayOrMayNotReShowHiddenWindows) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window2 = CreateTestWindow();
  auto window1 = CreateTestWindow();

  // If the dragged window restores to its original position, reshow the hidden
  // windows.
  StartDrag(window1.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  EXPECT_FALSE(window2->IsVisible());
  EndDrag(shelf_bounds.CenterPoint(), base::nullopt);
  EXPECT_TRUE(window2->IsVisible());

  // If fling to homescreen, do not reshow the hidden windows.
  StartDrag(window1.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  EXPECT_FALSE(window2->IsVisible());
  EndDrag(gfx::Point(200, 200),
          -DragWindowFromShelfController::kVelocityToHomeScreenThreshold);
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());

  // If the dragged window is added to overview, do not reshow the hidden
  // windows.
  window2->Show();
  window1->Show();
  StartDrag(window1.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  EXPECT_FALSE(window2->IsVisible());
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EndDrag(gfx::Point(200, 200), base::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
  EXPECT_FALSE(window2->IsVisible());
  overview_controller->EndOverview();

  // If the dragged window is snapped in splitview, while the other windows are
  // showing in overview, do not reshow the hidden windows.
  window2->Show();
  window1->Show();
  StartDrag(window1.get(), shelf_bounds.left_center(), HotseatState::kExtended);
  Drag(gfx::Point(0, 200), 0.f, 1.f);
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(gfx::Point(0, 200), base::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_FALSE(window2->IsVisible());
}

// Test during window dragging, if overview is open, the minimized windows can
// show correctly in overview.
TEST_F(DragWindowFromShelfControllerTest, MinimizedWindowsShowInOverview) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window3 = CreateTestWindow();
  auto window2 = CreateTestWindow();
  auto window1 = CreateTestWindow();

  StartDrag(window1.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  // Drag it far enough so overview should be open behind the dragged window.
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsMinimized());
  EXPECT_FALSE(window3->IsVisible());
  EXPECT_TRUE(WindowState::Get(window3.get())->IsMinimized());
  EXPECT_FALSE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window2.get()));
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window3.get()));
  // Release the drag, the window should be added to overview.
  EndDrag(gfx::Point(200, 200), base::nullopt);
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
}

// Test when swiping up from the shelf, we only open overview when the y scroll
// delta (velocity) decrease to kOpenOverviewThreshold or less.
TEST_F(DragWindowFromShelfControllerTest, OpenOverviewWhenHold) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateTestWindow();

  StartDrag(window.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 0.f,
       DragWindowFromShelfController::kOpenOverviewThreshold + 1);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  Drag(gfx::Point(200, 200), 0.f,
       DragWindowFromShelfController::kOpenOverviewThreshold);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(gfx::Point(200, 200), base::nullopt);
}

// Test if the dragged window is not dragged far enough than
// kReturnToMaximizedThreshold, it will restore back to its original position.
TEST_F(DragWindowFromShelfControllerTest, RestoreWindowToOriginalBounds) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateTestWindow();
  const gfx::Rect display_bounds = display::Screen::GetScreen()
                                       ->GetDisplayNearestWindow(window.get())
                                       .bounds();

  // Drag it for a small distance and then release.
  StartDrag(window.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 0.f,
       DragWindowFromShelfController::kShowOverviewThreshold + 1);
  EXPECT_FALSE(window->layer()->GetTargetTransform().IsIdentity());
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EndDrag(gfx::Point(200, 400), base::nullopt);
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());

  // Drag it for a large distance and then drag back to release.
  StartDrag(window.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  EXPECT_FALSE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EndDrag(
      gfx::Point(
          200,
          display_bounds.bottom() -
              DragWindowFromShelfController::GetReturnToMaximizedThreshold() +
              1),
      base::nullopt);
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());

  // The same thing should happen if splitview mode is active.
  auto window2 = CreateTestWindow();
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  StartDrag(window.get(), shelf_bounds.left_center(), HotseatState::kExtended);
  Drag(gfx::Point(0, 200), 0.f, 1.f);
  EXPECT_FALSE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(gfx::Point(0, 400), base::nullopt);
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(split_view_controller()->left_window(), window.get());
}

// Test if overview is active and splitview is not active, fling in overview may
// or may not head to the home screen.
TEST_F(DragWindowFromShelfControllerTest, FlingInOverview) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateTestWindow();

  // If fling velocity is smaller than kVelocityToHomeScreenThreshold, decide
  // where the window should go based on the release position.
  StartDrag(window.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(
      gfx::Point(0, 350),
      base::make_optional(
          -DragWindowFromShelfController::kVelocityToHomeScreenThreshold + 10));
  // The window should restore back to its original position.
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());

  // If fling velocity is equal or larger than kVelocityToHomeScreenThreshold
  StartDrag(window.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(gfx::Point(0, 350),
          base::make_optional(
              -DragWindowFromShelfController::kVelocityToHomeScreenThreshold));
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
}

// Verify that metrics of home launcher animation are recorded correctly when
// swiping up from shelf with sufficient velocity.
TEST_F(DragWindowFromShelfControllerTest, VerifyHomeLauncherAnimationMetrics) {
  // Set non-zero animation duration to report animation metrics.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateTestWindow();

  base::HistogramTester histogram_tester;

  // Ensure that fling velocity is sufficient to show homelauncher without
  // triggering overview mode.
  StartDrag(window.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 0.f,
       DragWindowFromShelfController::kOpenOverviewThreshold + 1);
  EndDrag(gfx::Point(0, 350),
          base::make_optional(
              -DragWindowFromShelfController::kVelocityToHomeScreenThreshold));
  WaitForHomeLauncherAnimationToFinish();

  // Verify that animation to show the home launcher is recorded.
  histogram_tester.ExpectTotalCount(
      "Apps.HomeLauncherTransition.AnimationSmoothness.FadeOutOverview", 1);
}

// Test if splitview is active when fling happens, the window will be put in
// overview.
TEST_F(DragWindowFromShelfControllerTest, DragOrFlingInSplitView) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();

  auto window1 = CreateTestWindow();
  auto window2 = CreateTestWindow();
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  // If the window is only dragged for a small distance:
  StartDrag(window1.get(), shelf_bounds.left_center(), HotseatState::kExtended);
  Drag(gfx::Point(100, 200), 0.f, 1.f);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(gfx::Point(100, 350), base::nullopt);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window2.get()));

  // If the window is dragged for a long distance:
  StartDrag(window1.get(), shelf_bounds.left_center(), HotseatState::kExtended);
  Drag(gfx::Point(100, 200), 0.f, 1.f);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(gfx::Point(100, 200), base::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window2.get()));
  overview_controller->EndOverview();

  // If the window is flung with a small velocity:
  StartDrag(window1.get(), shelf_bounds.left_center(), HotseatState::kExtended);
  Drag(gfx::Point(100, 200), 0.f, 1.f);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(
      gfx::Point(100, 350),
      base::make_optional(
          -DragWindowFromShelfController::kVelocityToOverviewThreshold + 10));
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window2.get()));

  // If the window is flung with a large velocity:
  StartDrag(window1.get(), shelf_bounds.left_center(), HotseatState::kExtended);
  Drag(gfx::Point(100, 200), 0.f, 1.f);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(gfx::Point(100, 350),
          base::make_optional(
              -DragWindowFromShelfController::kVelocityToOverviewThreshold));
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window2.get()));
  overview_controller->EndOverview();
}

// Test wallpaper should be blurred as in overview, even though overview might
// not open during dragging.
TEST_F(DragWindowFromShelfControllerTest, WallpaperBlurDuringDragging) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateTestWindow();

  StartDrag(window.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  Drag(gfx::Point(0, 200), 0.f,
       DragWindowFromShelfController::kShowOverviewThreshold + 1);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  auto* wallpaper_view =
      RootWindowController::ForWindow(window->GetRootWindow())
          ->wallpaper_widget_controller()
          ->wallpaper_view();
  EXPECT_EQ(wallpaper_view->property().blur_sigma,
            overview_constants::kBlurSigma);

  EndDrag(shelf_bounds.CenterPoint(),
          /*velocity_y=*/base::nullopt);
  EXPECT_EQ(wallpaper_view->property().blur_sigma,
            wallpaper_constants::kClear.blur_sigma);
}

// Test overview is hidden during dragging and shown when drag slows down or
// stops.
TEST_F(DragWindowFromShelfControllerTest, HideOverviewDuringDragging) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window2 = CreateTestWindow();
  auto window1 = CreateTestWindow();

  StartDrag(window1.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 0.5f, 0.5f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  // We test the visibility of overview by testing the drop target widget's
  // visibility in the overview.
  OverviewGrid* current_grid =
      overview_controller->overview_session()->GetGridWithRootWindow(
          window1->GetRootWindow());
  OverviewItem* drop_target_item = current_grid->GetDropTarget();
  EXPECT_TRUE(drop_target_item);
  EXPECT_EQ(drop_target_item->GetWindow()->layer()->GetTargetOpacity(), 1.f);

  Drag(gfx::Point(200, 200), 0.5f,
       DragWindowFromShelfController::kShowOverviewThreshold + 1);
  // Test overview should be invisble.
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(drop_target_item->GetWindow()->layer()->GetTargetOpacity(), 0.f);

  Drag(gfx::Point(200, 200), 0.5f, 0.5f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EndDrag(gfx::Point(200, 200),
          /*velocity_y=*/base::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  // |window1| should have added to overview. Test its visibility.
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
  EXPECT_EQ(window1->layer()->GetTargetOpacity(), 1.f);
}

// Check the split view drag indicators window dragging states.
TEST_F(DragWindowFromShelfControllerTest,
       SplitViewDragIndicatorsWindowDraggingStates) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateTestWindow();

  StartDrag(window.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 0.5f, 0.5f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  OverviewSession* overview_session = overview_controller->overview_session();
  ASSERT_EQ(1u, overview_session->grid_list().size());
  SplitViewDragIndicators* drag_indicators =
      overview_session->grid_list()[0]->split_view_drag_indicators();
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromShelf,
            drag_indicators->current_window_dragging_state());

  Drag(gfx::Point(0, 200), 0.5f, 0.5f);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapLeft,
            drag_indicators->current_window_dragging_state());

  Drag(gfx::Point(0, 300), 0.5f, 0.5f);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromShelf,
            drag_indicators->current_window_dragging_state());
  Drag(gfx::Point(0, 200), 0.5f, 0.5f);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapLeft,
            drag_indicators->current_window_dragging_state());
  Drag(gfx::Point(200, 200), 0.5f, 0.5f);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromShelf,
            drag_indicators->current_window_dragging_state());

  EndDrag(shelf_bounds.CenterPoint(),
          /*velocity_y=*/base::nullopt);
}

// Test there is no black backdrop behind the dragged window if we're doing the
// scale down animation for the dragged window.
TEST_F(DragWindowFromShelfControllerTest, NoBackdropDuringWindowScaleDown) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateTestWindow();
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
  WindowBackdrop* window_backdrop = WindowBackdrop::Get(window.get());
  EXPECT_NE(window_backdrop->mode(), WindowBackdrop::BackdropMode::kDisabled);

  StartDrag(window.get(), shelf_bounds.left_center(), HotseatState::kExtended);
  Drag(gfx::Point(0, 200), 0.f, 10.f);
  EndDrag(gfx::Point(0, 200),
          base::make_optional(
              -DragWindowFromShelfController::kVelocityToHomeScreenThreshold));
  EXPECT_FALSE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_NE(window_backdrop->mode(), WindowBackdrop::BackdropMode::kDisabled);
  EXPECT_TRUE(window_backdrop->temporarily_disabled());
}

// Test that if drag is cancelled, overview should be dismissed and other
// hidden windows should restore to its previous visibility state.
TEST_F(DragWindowFromShelfControllerTest, CancelDragDismissOverview) {
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window3 = CreateTestWindow();
  auto window2 = CreateTestWindow();
  auto window1 = CreateTestWindow();
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());

  StartDrag(window1.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 0.5f, 0.5f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());

  CancelDrag();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
}

TEST_F(DragWindowFromShelfControllerTest, CancelDragIfWindowDestroyed) {
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateTestWindow();
  StartDrag(window.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 0.5f, 0.5f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EXPECT_EQ(window_drag_controller()->dragged_window(), window.get());
  EXPECT_TRUE(window_drag_controller()->drag_started());
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kDragCanceled, 0);

  window.reset();
  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kDragCanceled, 1);

  EXPECT_EQ(window_drag_controller()->dragged_window(), nullptr);
  EXPECT_FALSE(window_drag_controller()->drag_started());
  // No crash should happen if Drag() call still comes in.
  Drag(gfx::Point(200, 200), 0.5f, 0.5f);
  CancelDrag();
}

TEST_F(DragWindowFromShelfControllerTest, FlingWithHiddenHotseat) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kHandleDragWindowFromShelfHistogramName,
      ShelfWindowDragResult::kRestoreToOriginalBounds, 0);

  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateTestWindow();
  gfx::Point start = shelf_bounds.CenterPoint();
  StartDrag(window.get(), start, HotseatState::kHidden);
  // Only drag for a small distance and then fling.
  Drag(gfx::Point(start.x(), start.y() - 10), 0.5f, 0.5f);
  EndDrag(gfx::Point(start.x(), start.y() - 10),
          base::make_optional(
              -DragWindowFromShelfController::kVelocityToHomeScreenThreshold));
  // The window should restore back to its original position.
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());

  histogram_tester.ExpectBucketCount(
      kHandleDragWindowFromShelfHistogramName,
      ShelfWindowDragResult::kRestoreToOriginalBounds, 1);

  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToHomeScreen, 0);

  // Now a bigger distance to fling.
  StartDrag(window.get(), start, HotseatState::kHidden);
  Drag(gfx::Point(start.x(), start.y() - 200), 0.5f, 0.5f);
  EndDrag(gfx::Point(start.x(), start.y() - 200),
          base::make_optional(
              -DragWindowFromShelfController::kVelocityToHomeScreenThreshold));
  // The window should be minimized.
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());

  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToHomeScreen, 1);
}

TEST_F(DragWindowFromShelfControllerTest, DragToSnapMinDistance) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();

  auto window1 = CreateTestWindow();
  auto window2 = CreateTestWindow();

  const gfx::Rect display_bounds = display::Screen::GetScreen()
                                       ->GetDisplayNearestWindow(window1.get())
                                       .bounds();
  const int snap_edge_inset =
      DragWindowFromShelfController::kScreenEdgeInsetForSnap;

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToOverviewMode,
                                     0);
  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToSplitviewMode,
                                     0);

  // If the drag starts outside of the snap region and then into snap region,
  // but the drag distance is not long enough.
  gfx::Point start = gfx::Point(display_bounds.x() + snap_edge_inset + 50,
                                shelf_bounds.CenterPoint().y());
  StartDrag(window1.get(), start, HotseatState::kExtended);
  Drag(start + gfx::Vector2d(0, 100), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  // Drag into the snap region and release.
  gfx::Point end = gfx::Point(
      start.x() - DragWindowFromShelfController::kMinDragDistance + 10, 200);
  EndDrag(end, base::nullopt);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToOverviewMode,
                                     1);
  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToSplitviewMode,
                                     0);

  wm::ActivateWindow(window1.get());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // If the drag starts outside of the snap region and then into snap region
  // (kScreenEdgeInsetForSnap), and the drag distance is long enough.
  StartDrag(window1.get(), start, HotseatState::kExtended);

  Drag(start + gfx::Vector2d(0, 100), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());

  // Drag into the snap region and release.
  end.set_x(start.x() - 10 - DragWindowFromShelfController::kMinDragDistance);
  EndDrag(end, base::nullopt);

  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));

  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToOverviewMode,
                                     1);
  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToSplitviewMode,
                                     1);

  WindowState::Get(window1.get())->Maximize();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // If the drag starts inside of the snap region (kScreenEdgeInsetForSnap), but
  // the drag distance is not long enough.
  start = gfx::Point(display_bounds.x() + snap_edge_inset - 5,
                     shelf_bounds.CenterPoint().y());
  StartDrag(window1.get(), start, HotseatState::kExtended);
  Drag(start + gfx::Vector2d(0, 100), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  // Drag for a small distance and release.
  end.set_x(start.x() - 10);
  EndDrag(end, base::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToOverviewMode,
                                     2);
  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToSplitviewMode,
                                     1);

  wm::ActivateWindow(window1.get());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // If the drag starts near the screen edge (kDistanceFromEdge), the window
  // should snap directly.
  start = gfx::Point(
      display_bounds.x() + DragWindowFromShelfController::kDistanceFromEdge - 5,
      shelf_bounds.CenterPoint().y());
  StartDrag(window1.get(), start, HotseatState::kExtended);
  Drag(start + gfx::Vector2d(0, 100), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  end.set_x(start.x() - 5);
  EndDrag(end, base::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));

  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToOverviewMode,
                                     2);
  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToSplitviewMode,
                                     2);
}

// Test that if overview is invisible when drag ends, the window will be taken
// to the home screen.
TEST_F(DragWindowFromShelfControllerTest, GoHomeIfOverviewInvisible) {
  UpdateDisplay("400x400");

  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateTestWindow();

  StartDrag(window.get(), shelf_bounds.left_center(), HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 0.f, 10.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  // End drag without any fling, the window should be added to overview.
  EndDrag(gfx::Point(200, 200), base::nullopt);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window.get()));

  wm::ActivateWindow(window.get());
  StartDrag(window.get(), shelf_bounds.left_center(), HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 0.f, 10.f);
  // At this moment overview should be invisible. End the drag without any
  // fling, the window should be taken to home screen.
  EndDrag(gfx::Point(200, 200), base::nullopt);
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
}

// Test that if overview is invisible when drag ends, the window will be taken
// to the home screen, even if drag satisfied min snap distance.
TEST_F(DragWindowFromShelfControllerTest,
       GoHomeIfOverviewInvisibleWithMinSnapDistance) {
  UpdateDisplay("400x400");

  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateTestWindow();

  const gfx::Rect display_bounds = display::Screen::GetScreen()
                                       ->GetDisplayNearestWindow(window.get())
                                       .bounds();
  int snap_edge_inset =
      display_bounds.width() * kHighlightScreenPrimaryAxisRatio +
      kHighlightScreenEdgePaddingDp;

  // Start the drag outside snap region.
  gfx::Point start = gfx::Point(display_bounds.x() + snap_edge_inset + 50,
                                shelf_bounds.CenterPoint().y());
  StartDrag(window.get(), start, HotseatState::kExtended);
  // Drag into the snap region and release without a fling.
  // At this moment overview should be invisible, so the window should be taken
  // to the home screen.
  gfx::Point end =
      start -
      gfx::Vector2d(10 + DragWindowFromShelfController::kMinDragDistance, 200);
  EndDrag(end, base::nullopt);

  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
}

// Test that the original backdrop is restored in the drag window after drag
// ends, no matter where the window ends.
TEST_F(DragWindowFromShelfControllerTest, RestoreBackdropAfterDragEnds) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateTestWindow();
  WindowBackdrop* window_backdrop = WindowBackdrop::Get(window.get());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);

  // For window that ends in overview:
  StartDrag(window.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  EXPECT_TRUE(window_backdrop->temporarily_disabled());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EndDrag(gfx::Point(200, 200), base::nullopt);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window.get()));
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);
  EXPECT_FALSE(window_backdrop->temporarily_disabled());

  // For window that ends in homescreen:
  wm::ActivateWindow(window.get());
  StartDrag(window.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  EXPECT_TRUE(window_backdrop->temporarily_disabled());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EndDrag(gfx::Point(200, 200),
          base::make_optional(
              -DragWindowFromShelfController::kVelocityToHomeScreenThreshold));
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
  EXPECT_FALSE(window_backdrop->temporarily_disabled());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);

  // For window that restores to its original bounds:
  wm::ActivateWindow(window.get());
  StartDrag(window.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  EXPECT_TRUE(window_backdrop->temporarily_disabled());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  EndDrag(shelf_bounds.CenterPoint(), base::nullopt);
  EXPECT_FALSE(window_backdrop->temporarily_disabled());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);

  // For window that ends in homescreen because overview did not start during
  // the gesture:
  wm::ActivateWindow(window.get());
  StartDrag(window.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  EXPECT_TRUE(window_backdrop->temporarily_disabled());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);
  EndDrag(gfx::Point(0, 200), base::nullopt);
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
  EXPECT_FALSE(window_backdrop->temporarily_disabled());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);

  // For window that ends in splitscreen:
  wm::ActivateWindow(window.get());
  StartDrag(window.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);
  EXPECT_TRUE(window_backdrop->temporarily_disabled());
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EndDrag(gfx::Point(0, 200), base::nullopt);
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window.get()));
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);
  EXPECT_FALSE(window_backdrop->temporarily_disabled());
}

TEST_F(DragWindowFromShelfControllerTest,
       DoNotChangeActiveWindowDuringDragging) {
  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateTestWindow();
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  StartDrag(window.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  // During dragging, the active window should not change.
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
  EndDrag(gfx::Point(200, 200), base::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  OverviewSession* overview_session = overview_controller->overview_session();
  EXPECT_TRUE(overview_session->IsWindowInOverview(window.get()));
  // After window is added to overview, the active window should change to the
  // overview focus widget.
  EXPECT_EQ(overview_session->GetOverviewFocusWindow(),
            window_util::GetActiveWindow());
}

// Test that if the window are dropped in overview before the overview start
// animation is completed, there is no crash.
TEST_F(DragWindowFromShelfControllerTest,
       NoCrashIfDropWindowInOverviewBeforeStartAnimationComplete) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->set_delayed_animation_task_delay_for_test(
      base::TimeDelta::FromMilliseconds(100));

  UpdateDisplay("400x400");
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
  auto window = CreateTestWindow();
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  StartDrag(window.get(), shelf_bounds.CenterPoint(), HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());

  EXPECT_TRUE(overview_controller->InOverviewSession());
  // During dragging, the active window should not change.
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
  OverviewSession* overview_session = overview_controller->overview_session();
  EndDrag(gfx::Point(200, 200), base::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_session->IsWindowInOverview(window.get()));
  // After window is added to overview, the active window should change to the
  // overview focus widget.
  EXPECT_EQ(overview_session->GetOverviewFocusWindow(),
            window_util::GetActiveWindow());

  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kEnterAnimationComplete);
  // After start animation is done, active window should remain the same.
  EXPECT_EQ(overview_session->GetOverviewFocusWindow(),
            window_util::GetActiveWindow());
}

// Test that when the dragged window is dropped into overview, it is positioned
// and stacked correctly.
TEST_F(DragWindowFromShelfControllerTest, DropsIntoOverviewAtCorrectPosition) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();
  ToggleOverview();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(gfx::ToRoundedPoint(
      GetOverviewItemForWindow(window1.get())->target_bounds().CenterPoint()));
  generator->DragMouseTo(0, 400);
  generator->MoveMouseTo(gfx::ToRoundedPoint(
      GetOverviewItemForWindow(window2.get())->target_bounds().CenterPoint()));
  generator->DragMouseTo(799, 400);
  EXPECT_EQ(window1.get(), split_view_controller()->left_window());
  EXPECT_EQ(window2.get(), split_view_controller()->right_window());
  ToggleOverview();
  StartDrag(window1.get(),
            Shelf::ForWindow(Shell::GetPrimaryRootWindow())
                ->GetIdealBounds()
                .left_center(),
            HotseatState::kExtended);
  Drag(gfx::Point(200, 200), 1.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EndDrag(gfx::Point(200, 200), base::nullopt);

  // Verify the grid arrangement.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  ASSERT_TRUE(overview_controller->InOverviewSession());
  const std::vector<aura::Window*> expected_mru_list = {
      window2.get(), window1.get(), window3.get()};
  const std::vector<aura::Window*> expected_overview_list = {
      window2.get(), window1.get(), window3.get()};
  EXPECT_EQ(
      expected_mru_list,
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk));
  EXPECT_EQ(expected_overview_list,
            overview_controller->GetWindowsListInOverviewGridsForTest());

  // Verify the stacking order.
  aura::Window* parent = window1->parent();
  ASSERT_EQ(parent, window2->parent());
  ASSERT_EQ(parent, window3->parent());
  EXPECT_GT(IndexOf(GetOverviewItemForWindow(window2.get())
                        ->item_widget()
                        ->GetNativeWindow(),
                    parent),
            IndexOf(GetOverviewItemForWindow(window1.get())
                        ->item_widget()
                        ->GetNativeWindow(),
                    parent));
  EXPECT_GT(IndexOf(GetOverviewItemForWindow(window1.get())
                        ->item_widget()
                        ->GetNativeWindow(),
                    parent),
            IndexOf(GetOverviewItemForWindow(window3.get())
                        ->item_widget()
                        ->GetNativeWindow(),
                    parent));
}

}  // namespace ash
