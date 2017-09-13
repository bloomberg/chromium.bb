// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_controller.h"

#include "ash/ash_switches.h"
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
#include "base/command_line.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {

class SplitViewControllerTest : public AshTestBase {
 public:
  SplitViewControllerTest() {}
  ~SplitViewControllerTest() override {}

  // test::AshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnableTabletSplitView);
    AshTestBase::SetUp();
    Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  }

  aura::Window* CreateWindow(const gfx::Rect& bounds) {
    aura::Window* window =
        CreateTestWindowInShellWithDelegate(&delegate_, -1, bounds);
    return window;
  }

  aura::Window* CreateNonSnappableWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateWindow(bounds);
    window->SetProperty(aura::client::kResizeBehaviorKey,
                        ui::mojom::kResizeBehaviorCanResize);
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

 private:
  aura::test::TestWindowDelegate delegate_;

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

// Tests that closing one of the snapped windows exits the split view mode.
TEST_F(SplitViewControllerTest, EndSplitViewUponWindowClose) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);

  window1.reset();
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
}

// Tests that if one of the snapped window gets minimized / maximized / full-
// screened, the split view mode ends.
TEST_F(SplitViewControllerTest, WindowStateChangeTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);

  wm::WMEvent minimize_event(wm::WM_EVENT_MINIMIZE);
  wm::GetWindowState(window1.get())->OnWMEvent(&minimize_event);
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
  std::vector<aura::Window*> windows = GetWindowsInOverviewGrids();
  auto iter = std::find(windows.begin(), windows.end(),
                        split_view_controller()->GetDefaultSnappedWindow());
  EXPECT_TRUE(iter == windows.end());

  // End overview mode before test shutdown to avoid use after free.
  ToggleOverview();
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
  EXPECT_FALSE(bounds_window1 == new_bounds_window1);
  EXPECT_FALSE(bounds_window2 == new_bounds_window2);
  EXPECT_FALSE(bounds_divider == new_bounds_divider);

  // Test that |window1|, divider, |window2| are still aligned properly.
  EXPECT_EQ(new_bounds_divider.x(),
            new_bounds_window1.x() + new_bounds_window1.width());
  EXPECT_EQ(new_bounds_window2.x(),
            new_bounds_divider.x() + new_bounds_divider.width());
}

// Verify the left and right windows get swapped when SwapWindows is called or
// the divider is double tapped.
TEST_F(SplitViewControllerTest, SwapWindows) {
  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());

  const gfx::Rect left_bounds = window1->GetBoundsInScreen();
  const gfx::Rect right_bounds = window2->GetBoundsInScreen();

  // Verify that after swapping windows, the windows and their bounds have been
  // swapped.
  split_view_controller()->SwapWindows();
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_EQ(left_bounds, window2->GetBoundsInScreen());
  EXPECT_EQ(right_bounds, window1->GetBoundsInScreen());

  // Perform a double tap on the divider center.
  gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  // The divider shifts after we click it once, so click it once before double
  // clicking.
  // TODO(sammiequon): Investigate why the divider shifts after the first click
  // and remove the extra click.
  GetEventGenerator().set_current_location(divider_center);
  GetEventGenerator().ClickLeftButton();
  divider_center = split_view_divider()
                       ->GetDividerBoundsInScreen(false /* is_dragging */)
                       .CenterPoint();
  GetEventGenerator().set_current_location(divider_center);
  GetEventGenerator().DoubleClickLeftButton();

  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
}

// Verifies that by long pressing on the overview button tray, split view gets
// activated iff we have two or more windows in the mru list.
TEST_F(SplitViewControllerTest, LongPressEntersSplitView) {
  // Verify that with no active windows, split view does not get activated.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());

  const gfx::Rect bounds(0, 0, 200, 200);
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
  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ASSERT_TRUE(split_view_controller()->IsSplitViewModeActive());

  // Verify that by long pressing on the overview button tray with one snapped
  // window, split view mode gets exited.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // Snap two windows.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  ASSERT_TRUE(split_view_controller()->IsSplitViewModeActive());

  // Verify that by long pressing on the overview button tray with two snapped
  // windows, split view mode gets exited.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
}

// Verify that split view mode get activated when long pressing on the overview
// button while in overview mode iff we have more than two windows in the mru
// list.
TEST_F(SplitViewControllerTest, LongPressInOverviewMode) {
  ToggleOverview();

  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  wm::ActivateWindow(window1.get());
  ASSERT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // Nothing happens if there is only one window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // Verify that with two windows, a long press on the overview button tray will
  // enter splitview.
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window2.get());
  LongPressOnOverivewButtonTray();
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(window2.get(), split_view_controller()->left_window());
}

TEST_F(SplitViewControllerTest, LongPressWithUnsnappableWindow) {
  // Add one unsnappable window and two regular windows.
  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> unsnappable_window(CreateWindow(bounds));
  unsnappable_window->SetProperty(aura::client::kResizeBehaviorKey,
                                  ui::mojom::kResizeBehaviorNone);
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
  ToggleOverview();
}

}  // namespace ash
