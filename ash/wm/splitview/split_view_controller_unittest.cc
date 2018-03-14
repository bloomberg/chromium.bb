// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_controller.h"

#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/stl_util.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class SplitViewControllerTest : public AshTestBase {
 public:
  SplitViewControllerTest() = default;
  ~SplitViewControllerTest() override = default;

  // test::AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  }

  aura::Window* CreateWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowInShellWithDelegate(
        new SplitViewTestWindowDelegate, -1, bounds);
    return window;
  }

  aura::Window* CreateNonSnappableWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateWindow(bounds);
    window->SetProperty(aura::client::kResizeBehaviorKey,
                        ui::mojom::kResizeBehaviorNone);
    return window;
  }

  void EndSplitView() { split_view_controller()->EndSplitView(); }

  void ToggleOverview() {
    Shell::Get()->window_selector_controller()->ToggleOverview();
  }

  void LongPressOnOverivewButtonTray() {
    ui::GestureEvent event(0, 0, 0, base::TimeTicks(),
                           ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
    StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->overview_button_tray()
        ->OnGestureEvent(&event);
  }

  std::vector<aura::Window*> GetWindowsInOverviewGrids() {
    return Shell::Get()
        ->window_selector_controller()
        ->GetWindowsListInOverviewGridsForTesting();
  }

  SplitViewController* split_view_controller() {
    return Shell::Get()->split_view_controller();
  }

  SplitViewDivider* split_view_divider() {
    return split_view_controller()->split_view_divider();
  }

  blink::WebScreenOrientationLockType screen_orientation() {
    return split_view_controller()->GetCurrentScreenOrientation();
  }

  int divider_position() { return split_view_controller()->divider_position(); }

 private:
  class SplitViewTestWindowDelegate : public aura::test::TestWindowDelegate {
   public:
    SplitViewTestWindowDelegate() = default;
    ~SplitViewTestWindowDelegate() override = default;

    // aura::test::TestWindowDelegate:
    void OnWindowDestroying(aura::Window* window) override { window->Hide(); }
    void OnWindowDestroyed(aura::Window* window) override { delete this; }
  };

  DISALLOW_COPY_AND_ASSIGN(SplitViewControllerTest);
};

// Tests the basic functionalities.
TEST_F(SplitViewControllerTest, Basic) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  EXPECT_EQ(split_view_controller()->state(), SplitViewController::NO_SNAP);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_NE(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(window1->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                window1.get(), SplitViewController::LEFT));

  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
  EXPECT_NE(split_view_controller()->right_window(), window1.get());
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(window2->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                window2.get(), SplitViewController::RIGHT));

  EndSplitView();
  EXPECT_EQ(split_view_controller()->state(), SplitViewController::NO_SNAP);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
}

// Tests that the default snapped window is the first window that gets snapped.
TEST_F(SplitViewControllerTest, DefaultSnappedWindow) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(window1.get(), split_view_controller()->GetDefaultSnappedWindow());

  EndSplitView();
  split_view_controller()->SnapWindow(window2.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(window2.get(), split_view_controller()->GetDefaultSnappedWindow());
}

// Tests that if there are two snapped windows, closing one of them will open
// overview window grid on the closed window side of the screen. If there is
// only one snapped windows, closing the sanpped window will end split view mode
// and adjust the overview window grid bounds if the overview mode is active at
// that moment.
TEST_F(SplitViewControllerTest, WindowCloseTest) {
  // 1 - First test one snapped window scenario.
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window0(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
  split_view_controller()->SnapWindow(window0.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  // Closing this snapped window should exit split view mode.
  window0.reset();
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  // 2 - Then test two snapped windows scenario.
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::LEFT);

  // Closing one of the two snapped windows will not end split view mode.
  window1.reset();
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::RIGHT_SNAPPED);
  // Since left window was closed, its default snap position changed to RIGHT.
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::RIGHT);
  // Window grid is showing no recent items, and has no windows, but it is still
  // available.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Now close the other snapped window.
  window2.reset();
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
  EXPECT_EQ(split_view_controller()->state(), SplitViewController::NO_SNAP);
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // 3 - Then test the scenario with more than two windows.
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window5(CreateWindow(bounds));
  split_view_controller()->SnapWindow(window3.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window4.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::LEFT);

  // Close one of the snapped windows.
  window4.reset();
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::LEFT);
  // Now overview window grid can be opened.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Close the other snapped window.
  window3.reset();
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
  EXPECT_EQ(split_view_controller()->state(), SplitViewController::NO_SNAP);
  // Test the overview winow grid should still open.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
}

// Tests that if there are two snapped windows, minimizing one of them will open
// overview window grid on the minimized window side of the screen. If there is
// only one snapped windows, minimizing the sanpped window will end split view
// mode and adjust the overview window grid bounds if the overview mode is
// active at that moment.
TEST_F(SplitViewControllerTest, MinimizeWindowTest) {
  const gfx::Rect bounds(0, 0, 400, 400);

  // 1 - First test one snapped window scenario.
  std::unique_ptr<aura::Window> window0(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
  split_view_controller()->SnapWindow(window0.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  wm::WMEvent minimize_event(wm::WM_EVENT_MINIMIZE);
  wm::GetWindowState(window0.get())->OnWMEvent(&minimize_event);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  // 2 - Then test the scenario that has 2 or more windows.
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::LEFT);

  // Minimizing one of the two snapped windows will not end split view mode.
  wm::GetWindowState(window1.get())->OnWMEvent(&minimize_event);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::RIGHT_SNAPPED);
  // Since left window was minimized, its default snap position changed to
  // RIGHT.
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::RIGHT);
  // The overview window grid will open.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Now minimize the other snapped window.
  wm::GetWindowState(window2.get())->OnWMEvent(&minimize_event);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
  EXPECT_EQ(split_view_controller()->state(), SplitViewController::NO_SNAP);
  // The overview window grid is still open.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
}

// Tests that if one of the snapped window gets maximized / full-screened, the
// split view mode ends.
TEST_F(SplitViewControllerTest, WindowStateChangeTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  // 1 - First test one snapped window scenario.
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);

  wm::WMEvent maximize_event(wm::WM_EVENT_MAXIMIZE);
  wm::GetWindowState(window1.get())->OnWMEvent(&maximize_event);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);

  wm::WMEvent fullscreen_event(wm::WM_EVENT_FULLSCREEN);
  wm::GetWindowState(window1.get())->OnWMEvent(&fullscreen_event);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  // 2 - Then test two snapped window scenario.
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);

  // Maximize one of the snapped window will end the split view mode.
  wm::GetWindowState(window1.get())->OnWMEvent(&maximize_event);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);

  // Full-screen one of the snapped window will also end the split view mode.
  wm::GetWindowState(window1.get())->OnWMEvent(&fullscreen_event);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  // 3 - Test the scenario that part of the screen is a snapped window and part
  // of the screen is the overview window grid.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  ToggleOverview();
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);

  // Maximize the snapped window will end the split view mode and overview mode.
  wm::GetWindowState(window1.get())->OnWMEvent(&maximize_event);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  ToggleOverview();
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);

  // Fullscreen the snapped window will end the split view mode and overview
  // mode.
  wm::GetWindowState(window1.get())->OnWMEvent(&fullscreen_event);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
}

// Tests that if split view mode is active, activate another window will snap
// the window to the non-default side of the screen.
TEST_F(SplitViewControllerTest, WindowActivationTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);

  wm::ActivateWindow(window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);

  wm::ActivateWindow(window3.get());
  EXPECT_EQ(split_view_controller()->right_window(), window3.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
}

// Tests that if split view mode and overview mode are active at the same time,
// i.e., half of the screen is occupied by a snapped window and half of the
// screen is occupied by the overview windows grid, the next activatable window
// will be picked to snap when exiting the overview mode.
TEST_F(SplitViewControllerTest, ExitOverviewTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());

  ToggleOverview();
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->right_window(), window3.get());
}

// Tests that if split view mode is active when entering overview, the overview
// windows grid should show in the non-default side of the screen, and the
// default snapped window should not be shown in the overview window grid.
TEST_F(SplitViewControllerTest, EnterOverviewTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->GetDefaultSnappedWindow(), window1.get());

  ToggleOverview();
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_FALSE(
      base::ContainsValue(GetWindowsInOverviewGrids(),
                          split_view_controller()->GetDefaultSnappedWindow()));
}

// Tests that the split divider was created when the split view mode is active
// and destroyed when the split view mode is ended. The split divider should be
// always above the two snapped windows.
TEST_F(SplitViewControllerTest, SplitDividerBasicTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  EXPECT_TRUE(!split_view_divider());
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_TRUE(split_view_divider());
  EXPECT_TRUE(split_view_divider()->divider_widget()->IsAlwaysOnTop());
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_TRUE(split_view_divider());
  EXPECT_TRUE(split_view_divider()->divider_widget()->IsAlwaysOnTop());

  std::unique_ptr<aura::Window> window3(CreateNonSnappableWindow(bounds));
  wm::ActivateWindow(window3.get());
  EXPECT_TRUE(split_view_divider());
  EXPECT_FALSE(split_view_divider()->divider_widget()->IsAlwaysOnTop());

  EndSplitView();
  EXPECT_TRUE(!split_view_divider());
}

// Verifys that the bounds of the two windows in splitview are as expected.
TEST_F(SplitViewControllerTest, SplitDividerWindowBounds) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  ASSERT_TRUE(split_view_divider());

  // Verify with two freshly snapped windows are roughly the same width (off by
  // one pixel at most due to the display maybe being even and the divider being
  // a fixed odd pixel width).
  int window1_width = window1->GetBoundsInScreen().width();
  int window2_width = window2->GetBoundsInScreen().width();
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  const int screen_width =
      screen_util::GetDisplayWorkAreaBoundsInParent(window1.get()).width();
  EXPECT_NEAR(window1_width, window2_width, 1);
  EXPECT_EQ(screen_width,
            window1_width + divider_bounds.width() + window2_width);

  // Drag the divider to a position two thirds of the screen size. Verify window
  // 1 is wider than window 2.
  GetEventGenerator().set_current_location(divider_bounds.CenterPoint());
  GetEventGenerator().DragMouseTo(screen_width * 0.67f, 0);
  window1_width = window1->GetBoundsInScreen().width();
  window2_width = window2->GetBoundsInScreen().width();
  const int old_window1_width = window1_width;
  const int old_window2_width = window2_width;
  EXPECT_GT(window1_width, 2 * window2_width);
  EXPECT_EQ(screen_width,
            window1_width + divider_bounds.width() + window2_width);

  // Drag the divider to a position close to two thirds of the screen size.
  // Verify the divider snaps to two thirds of the screen size, and the windows
  // remain the same size as previously.
  divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  GetEventGenerator().set_current_location(divider_bounds.CenterPoint());
  GetEventGenerator().DragMouseTo(screen_width * 0.7f, 0);
  window1_width = window1->GetBoundsInScreen().width();
  window2_width = window2->GetBoundsInScreen().width();
  EXPECT_EQ(window1_width, old_window1_width);
  EXPECT_EQ(window2_width, old_window2_width);

  // Drag the divider to a position one third of the screen size. Verify window
  // 1 is wider than window 2.
  divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  GetEventGenerator().set_current_location(divider_bounds.CenterPoint());
  GetEventGenerator().DragMouseTo(screen_width * 0.33f, 0);
  window1_width = window1->GetBoundsInScreen().width();
  window2_width = window2->GetBoundsInScreen().width();
  EXPECT_GT(window2_width, 2 * window1_width);
  EXPECT_EQ(screen_width,
            window1_width + divider_bounds.width() + window2_width);

  // Verify that the left window from dragging the divider to two thirds of the
  // screen size is roughly the same size as the right window after dragging the
  // divider to one third of the screen size, and vice versa.
  EXPECT_NEAR(window1_width, old_window2_width, 1);
  EXPECT_NEAR(window2_width, old_window1_width, 1);
}

// Tests that the bounds of the snapped windows and divider are adjusted when
// the screen display configuration changes.
TEST_F(SplitViewControllerTest, DisplayConfigurationChangeTest) {
  UpdateDisplay("407x400");
  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);

  const gfx::Rect bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1| and |window2| has the same width and height after snap.
  EXPECT_EQ(bounds_window1.width(), bounds_window2.width());
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());
  EXPECT_EQ(bounds_divider.height(), bounds_window1.height());

  // Test that |window1|, divider, |window2| are aligned properly.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());

  // Now change the display configuration.
  UpdateDisplay("507x500");
  const gfx::Rect new_bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect new_bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect new_bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that the new bounds are different with the old ones.
  EXPECT_NE(bounds_window1, new_bounds_window1);
  EXPECT_NE(bounds_window2, new_bounds_window2);
  EXPECT_NE(bounds_divider, new_bounds_divider);

  // Test that |window1|, divider, |window2| are still aligned properly.
  EXPECT_EQ(new_bounds_divider.x(),
            new_bounds_window1.x() + new_bounds_window1.width());
  EXPECT_EQ(new_bounds_window2.x(),
            new_bounds_divider.x() + new_bounds_divider.width());
}

// Verify the left and right windows get swapped when SwapWindows is called or
// the divider is double tapped.
TEST_F(SplitViewControllerTest, SwapWindows) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());

  gfx::Rect left_bounds = window1->GetBoundsInScreen();
  gfx::Rect right_bounds = window2->GetBoundsInScreen();

  // Verify that after swapping windows, the windows and their bounds have been
  // swapped.
  split_view_controller()->SwapWindows();
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_EQ(left_bounds, window2->GetBoundsInScreen());
  EXPECT_EQ(right_bounds, window1->GetBoundsInScreen());

  // End split view mode and snap the window to RIGHT first, verify the function
  // SwapWindows() still works properly.
  EndSplitView();
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  split_view_controller()->SnapWindow(window2.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());

  left_bounds = window2->GetBoundsInScreen();
  right_bounds = window1->GetBoundsInScreen();

  split_view_controller()->SwapWindows();
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
  EXPECT_EQ(left_bounds, window1->GetBoundsInScreen());
  EXPECT_EQ(right_bounds, window2->GetBoundsInScreen());

  // Perform a double tap on the divider center.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  GetEventGenerator().set_current_location(divider_center);
  GetEventGenerator().DoubleClickLeftButton();

  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_EQ(left_bounds, window2->GetBoundsInScreen());
  EXPECT_EQ(right_bounds, window1->GetBoundsInScreen());
}

// Verifies that by long pressing on the overview button tray, split view gets
// activated iff we have two or more windows in the mru list.
TEST_F(SplitViewControllerTest, LongPressEntersSplitView) {
  // Verify that with no active windows, split view does not get activated.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());

  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  wm::ActivateWindow(window1.get());

  // Verify that with only one window, split view does not get activated.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // Verify that with two windows, split view gets activated.
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window2.get());
  LongPressOnOverivewButtonTray();
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
}

// Verify that when in split view mode with either one snapped or two snapped
// windows, split view mode gets exited when the overview button gets a long
// press event.
TEST_F(SplitViewControllerTest, LongPressExitsSplitView) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  // Snap |window1| to the left.
  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ASSERT_TRUE(split_view_controller()->IsSplitViewModeActive());

  // Verify that by long pressing on the overview button tray with left snapped
  // window, split view mode gets exited and the left window (|window1|) is the
  // current active window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(window1.get(), wm::GetActiveWindow());

  // Snap |window1| to the right.
  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  ASSERT_TRUE(split_view_controller()->IsSplitViewModeActive());

  // Verify that by long pressing on the overview button tray with right snapped
  // window, split view mode gets exited and the right window (|window1|) is the
  // current active window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(window1.get(), wm::GetActiveWindow());

  // Snap two windows and activate the left window, |window1|.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  wm::ActivateWindow(window1.get());
  ASSERT_TRUE(split_view_controller()->IsSplitViewModeActive());

  // Verify that by long pressing on the overview button tray with two snapped
  // windows, split view mode gets exited.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(window1.get(), wm::GetActiveWindow());

  // Snap two windows and activate the right window, |window2|.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  wm::ActivateWindow(window2.get());
  ASSERT_TRUE(split_view_controller()->IsSplitViewModeActive());

  // Verify that by long pressing on the overview button tray with two snapped
  // windows, split view mode gets exited, and the activated window in splitview
  // is the current active window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(window2.get(), wm::GetActiveWindow());
}

TEST_F(SplitViewControllerTest, LongPressExitsSplitViewWithTransientChild) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> left_window(CreateWindow(bounds));
  std::unique_ptr<aura::Window> right_window(CreateWindow(bounds));
  wm::ActivateWindow(left_window.get());
  wm::ActivateWindow(right_window.get());

  ToggleOverview();
  split_view_controller()->SnapWindow(left_window.get(),
                                      SplitViewController::LEFT);
  split_view_controller()->SnapWindow(right_window.get(),
                                      SplitViewController::RIGHT);
  ASSERT_TRUE(split_view_controller()->IsSplitViewModeActive());

  // Add a transient child to |right_window|, and activate it.
  aura::Window* transient_child =
      aura::test::CreateTestWindowWithId(0, right_window.get());
  ::wm::AddTransientChild(right_window.get(), transient_child);
  wm::ActivateWindow(transient_child);

  // Verify that by long pressing on the overview button tray, split view mode
  // gets exited and the window which contained |transient_child| is the
  // current active window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(right_window.get(), wm::GetActiveWindow());
}

// Verify that split view mode get activated when long pressing on the overview
// button while in overview mode iff we have more than two windows in the mru
// list.
TEST_F(SplitViewControllerTest, LongPressInOverviewMode) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  wm::ActivateWindow(window1.get());

  ToggleOverview();
  ASSERT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  ASSERT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // Nothing happens if there is only one window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // Verify that with two windows, a long press on the overview button tray will
  // enter splitview.
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window2.get());
  ToggleOverview();
  ASSERT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  ASSERT_FALSE(split_view_controller()->IsSplitViewModeActive());

  LongPressOnOverivewButtonTray();
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(window2.get(), split_view_controller()->left_window());
}

TEST_F(SplitViewControllerTest, LongPressWithUnsnappableWindow) {
  // Add one unsnappable window and two regular windows.
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> unsnappable_window(
      CreateNonSnappableWindow(bounds));
  ASSERT_FALSE(split_view_controller()->IsSplitViewModeActive());
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(unsnappable_window.get());
  ASSERT_EQ(unsnappable_window.get(), wm::GetActiveWindow());

  // Verify split view is not activated when long press occurs when active
  // window is unsnappable.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // Verify split view is not activated when long press occurs in overview mode
  // and the most recent window is unsnappable.
  ToggleOverview();
  ASSERT_TRUE(Shell::Get()->mru_window_tracker()->BuildMruWindowList().size() >
              0);
  ASSERT_EQ(unsnappable_window.get(),
            Shell::Get()->mru_window_tracker()->BuildMruWindowList()[0]);
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
}

// Test the rotation functionalities in split view mode.
TEST_F(SplitViewControllerTest, RotationTest) {
  UpdateDisplay("807x407");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  // Set the screen orientation to LANDSCAPE_PRIMARY.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            blink::kWebScreenOrientationLockLandscapePrimary);

  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  gfx::Rect bounds_window1 = window1->GetBoundsInScreen();
  gfx::Rect bounds_window2 = window2->GetBoundsInScreen();
  gfx::Rect bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test |window1|, divider and |window2| are aligned horizontally.
  // |window1| is on the left, then the divider, and then |window2|.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());
  EXPECT_EQ(bounds_window1.height(), bounds_divider.height());
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());

  // Rotate the screen by 270 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            blink::kWebScreenOrientationLockPortraitPrimary);

  bounds_window1 = window1->GetBoundsInScreen();
  bounds_window2 = window2->GetBoundsInScreen();
  bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1|, divider, |window2| are now aligned vertically.
  // |window1| is on the top, then the divider, and then |window2|.
  EXPECT_EQ(bounds_divider.y(), bounds_window1.y() + bounds_window1.height());
  EXPECT_EQ(bounds_window2.y(), bounds_divider.y() + bounds_divider.height());
  EXPECT_EQ(bounds_window1.width(), bounds_divider.width());
  EXPECT_EQ(bounds_window1.width(), bounds_window2.width());

  // Rotate the screen by 180 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            blink::kWebScreenOrientationLockLandscapeSecondary);

  bounds_window1 = window1->GetBoundsInScreen();
  bounds_window2 = window2->GetBoundsInScreen();
  bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1|, divider, |window2| are now aligned horizontally.
  // |window2| is on the left, then the divider, and then |window1|.
  EXPECT_EQ(bounds_divider.x(), bounds_window2.x() + bounds_window2.width());
  EXPECT_EQ(bounds_window1.x(), bounds_divider.x() + bounds_divider.width());
  EXPECT_EQ(bounds_window1.height(), bounds_divider.height());
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());

  // Rotate the screen by 90 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            blink::kWebScreenOrientationLockPortraitSecondary);
  bounds_window1 = window1->GetBoundsInScreen();
  bounds_window2 = window2->GetBoundsInScreen();
  bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1|, divider, |window2| are now aligned vertically.
  // |window2| is on the top, then the divider, and then |window1|.
  EXPECT_EQ(bounds_divider.y(), bounds_window2.y() + bounds_window2.height());
  EXPECT_EQ(bounds_window1.y(), bounds_divider.y() + bounds_divider.height());
  EXPECT_EQ(bounds_window1.width(), bounds_divider.width());
  EXPECT_EQ(bounds_window1.width(), bounds_window2.width());

  // Rotate the screen back to 0 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            blink::kWebScreenOrientationLockLandscapePrimary);
  bounds_window1 = window1->GetBoundsInScreen();
  bounds_window2 = window2->GetBoundsInScreen();
  bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test |window1|, divider and |window2| are aligned horizontally.
  // |window1| is on the left, then the divider, and then |window2|.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());
  EXPECT_EQ(bounds_window1.height(), bounds_divider.height());
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());
}

// Test that if the split view mode is active when exiting tablet mode, we
// should also end split view mode.
TEST_F(SplitViewControllerTest, ExitTabletModeEndSplitView) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());

  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
}

// Tests that if a window's minimum size is larger than half of the display work
// area's size, it can't be snapped.
TEST_F(SplitViewControllerTest, SnapWindowWithMinimumSizeTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  EXPECT_TRUE(split_view_controller()->CanSnap(window1.get()));

  const gfx::Rect display_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window1.get());
  aura::test::TestWindowDelegate* delegate =
      static_cast<aura::test::TestWindowDelegate*>(window1->delegate());
  delegate->set_minimum_size(
      gfx::Size(display_bounds.width() * 0.5f, display_bounds.height()));
  EXPECT_TRUE(split_view_controller()->CanSnap(window1.get()));

  delegate->set_minimum_size(
      gfx::Size(display_bounds.width() * 0.67f, display_bounds.height()));
  EXPECT_FALSE(split_view_controller()->CanSnap(window1.get()));
}

// Tests that the snapped window can not be moved outside of work area when its
// minimum size is larger than its current desired resizing bounds.
TEST_F(SplitViewControllerTest, ResizingSnappedWindowWithMinimumSizeTest) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  const gfx::Rect bounds(0, 0, 300, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate1 =
      static_cast<aura::test::TestWindowDelegate*>(window1->delegate());

  // Set the screen orientation to LANDSCAPE_PRIMARY
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            blink::kWebScreenOrientationLockLandscapePrimary);

  gfx::Rect display_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window1.get());
  EXPECT_TRUE(split_view_controller()->CanSnap(window1.get()));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  delegate1->set_minimum_size(
      gfx::Size(display_bounds.width() * 0.4f, display_bounds.height()));
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());

  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  gfx::Point resize_point(display_bounds.width() * 0.33f, 0);
  split_view_controller()->Resize(resize_point);

  gfx::Rect snapped_window_bounds =
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          window1.get(), SplitViewController::LEFT);
  // The snapped window bounds can't be pushed outside of the display area.
  EXPECT_EQ(snapped_window_bounds.x(), display_bounds.x());
  EXPECT_EQ(snapped_window_bounds.width(),
            window1->delegate()->GetMinimumSize().width());
  EXPECT_FALSE(window1->layer()->GetTargetTransform().IsIdentity());
  EndSplitView();

  // Rotate the screen by 270 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            blink::kWebScreenOrientationLockPortraitPrimary);

  display_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window1.get());
  delegate1->set_minimum_size(
      gfx::Size(display_bounds.width(), display_bounds.height() * 0.4f));
  EXPECT_TRUE(split_view_controller()->CanSnap(window1.get()));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  resize_point.SetPoint(0, display_bounds.height() * 0.33f);
  split_view_controller()->Resize(resize_point);

  snapped_window_bounds =
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(snapped_window_bounds.y(), display_bounds.y());
  EXPECT_EQ(snapped_window_bounds.height(),
            window1->delegate()->GetMinimumSize().height());
  EXPECT_FALSE(window1->layer()->GetTargetTransform().IsIdentity());
  EndSplitView();

  // Rotate the screen by 180 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            blink::kWebScreenOrientationLockLandscapeSecondary);

  display_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window1.get());
  delegate1->set_minimum_size(
      gfx::Size(display_bounds.width() * 0.4f, display_bounds.height()));
  EXPECT_TRUE(split_view_controller()->CanSnap(window1.get()));
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());

  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  resize_point.SetPoint(display_bounds.width() * 0.33f, 0);
  split_view_controller()->Resize(resize_point);

  snapped_window_bounds =
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          window1.get(), SplitViewController::RIGHT);
  EXPECT_EQ(snapped_window_bounds.x(), display_bounds.x());
  EXPECT_EQ(snapped_window_bounds.width(),
            window1->delegate()->GetMinimumSize().width());
  EXPECT_FALSE(window1->layer()->GetTargetTransform().IsIdentity());
  EndSplitView();

  // Rotate the screen by 90 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            blink::kWebScreenOrientationLockPortraitSecondary);

  display_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window1.get());
  delegate1->set_minimum_size(
      gfx::Size(display_bounds.width(), display_bounds.height() * 0.4f));
  EXPECT_TRUE(split_view_controller()->CanSnap(window1.get()));
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());

  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  resize_point.SetPoint(0, display_bounds.height() * 0.33f);
  split_view_controller()->Resize(resize_point);

  snapped_window_bounds =
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          window1.get(), SplitViewController::RIGHT);
  EXPECT_EQ(snapped_window_bounds.y(), display_bounds.y());
  EXPECT_EQ(snapped_window_bounds.height(),
            window1->delegate()->GetMinimumSize().height());
  EXPECT_FALSE(window1->layer()->GetTargetTransform().IsIdentity());
  EndSplitView();
}

// Tests that the divider should not be moved to a position that is smaller than
// the snapped window's minimum size after resizing.
TEST_F(SplitViewControllerTest,
       DividerPositionOnResizingSnappedWindowWithMinimumSizeTest) {
  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate1 =
      static_cast<aura::test::TestWindowDelegate*>(window1->delegate());
  ui::test::EventGenerator& generator(GetEventGenerator());
  EXPECT_EQ(blink::kWebScreenOrientationLockLandscapePrimary,
            screen_orientation());
  gfx::Rect workarea_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window1.get());

  // Snap the divider to one third position when there is only left window with
  // minimum size larger than one third of the display's width. The divider
  // should be snapped to the middle position after dragging.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  delegate1->set_minimum_size(
      gfx::Size(workarea_bounds.width() * 0.4f, workarea_bounds.height()));
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  generator.set_current_location(divider_bounds.CenterPoint());
  generator.DragMouseTo(gfx::Point(workarea_bounds.width() * 0.33f, 0));
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());

  // Snap the divider to two third position, it should be kept at there after
  // dragging.
  generator.set_current_location(divider_bounds.CenterPoint());
  generator.DragMouseTo(gfx::Point(workarea_bounds.width() * 0.67f, 0));
  EXPECT_GT(divider_position(), 0.5f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.67f * workarea_bounds.width());
  EndSplitView();

  // Snap the divider to two third position when there is only right window with
  // minium size larger than one third of the display's width. The divider
  // should be snapped to the middle position after dragging.
  delegate1->set_minimum_size(
      gfx::Size(workarea_bounds.width() * 0.4f, workarea_bounds.height()));
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  generator.set_current_location(divider_bounds.CenterPoint());
  generator.DragMouseTo(gfx::Point(workarea_bounds.width() * 0.67f, 0));
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());

  // Snap the divider to one third position, it should be kept at there after
  // dragging.
  generator.set_current_location(divider_bounds.CenterPoint());
  generator.DragMouseTo(gfx::Point(workarea_bounds.width() * 0.33f, 0));
  EXPECT_GT(divider_position(), 0);
  EXPECT_LE(divider_position(), 0.33f * workarea_bounds.width());
  EndSplitView();

  // Snap the divider to one third position when there are both left and right
  // snapped windows with the same minimum size larger than one third of the
  // display's width. The divider should be snapped to the middle position after
  // dragging.
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate2 =
      static_cast<aura::test::TestWindowDelegate*>(window2->delegate());
  delegate2->set_minimum_size(
      gfx::Size(workarea_bounds.width() * 0.4f, workarea_bounds.height()));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  generator.set_current_location(divider_bounds.CenterPoint());
  generator.DragMouseTo(gfx::Point(workarea_bounds.width() * 0.33f, 0));
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());

  // Snap the divider to two third position, it should be snapped to the middle
  // position after dragging.
  generator.set_current_location(divider_bounds.CenterPoint());
  generator.DragMouseTo(gfx::Point(workarea_bounds.width() * 0.67f, 0));
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());
  EndSplitView();
}

// Tests that the divider and snapped windows bounds should be updated if
// snapping a new window with minimum size, which is larger than the bounds
// of its snap position.
TEST_F(SplitViewControllerTest,
       DividerPositionWithWindowMinimumSizeOnSnapTest) {
  const gfx::Rect bounds(0, 0, 200, 300);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  const gfx::Rect workarea_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window1.get());

  // Divider should be moved to the middle at the beginning.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ASSERT_TRUE(split_view_divider());
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());

  // Drag the divider to two-third position.
  ui::test::EventGenerator& generator(GetEventGenerator());
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  generator.set_current_location(divider_bounds.CenterPoint());
  generator.DragMouseTo(gfx::Point(workarea_bounds.width() * 0.67f, 0));
  EXPECT_GT(divider_position(), 0.5f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.67f * workarea_bounds.width());

  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate2 =
      static_cast<aura::test::TestWindowDelegate*>(window2->delegate());
  delegate2->set_minimum_size(
      gfx::Size(workarea_bounds.width() * 0.4f, workarea_bounds.height()));
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());
}

}  // namespace ash
