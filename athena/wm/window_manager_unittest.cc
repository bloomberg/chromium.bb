// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/public/window_manager.h"

#include "athena/screen/public/screen_manager.h"
#include "athena/test/athena_test_base.h"
#include "athena/wm/public/window_list_provider.h"
#include "athena/wm/split_view_controller.h"
#include "athena/wm/test/window_manager_impl_test_api.h"
#include "athena/wm/window_manager_impl.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/wm/core/window_util.h"

namespace athena {

class WindowManagerTest : public test::AthenaTestBase {
 public:
  WindowManagerTest() {}
  virtual ~WindowManagerTest() {}

  scoped_ptr<aura::Window> CreateAndActivateWindow(
      aura::WindowDelegate* delegate) {
    scoped_ptr<aura::Window> window(CreateTestWindow(delegate, gfx::Rect()));
    window->Show();
    wm::ActivateWindow(window.get());
    return window.Pass();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowManagerTest);
};

TEST_F(WindowManagerTest, OverviewModeBasics) {
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> first(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> second(CreateAndActivateWindow(&delegate));

  test::WindowManagerImplTestApi wm_api;
  wm::ActivateWindow(second.get());

  ASSERT_FALSE(WindowManager::Get()->IsOverviewModeActive());
  EXPECT_EQ(first->bounds().ToString(), second->bounds().ToString());
  EXPECT_EQ(gfx::Screen::GetNativeScreen()
                ->GetPrimaryDisplay()
                .work_area()
                .size()
                .ToString(),
            first->bounds().size().ToString());
  EXPECT_FALSE(WindowManager::Get()->IsOverviewModeActive());

  // Tests that going into overview mode does not change the window bounds.
  WindowManager::Get()->ToggleOverview();
  ASSERT_TRUE(WindowManager::Get()->IsOverviewModeActive());
  EXPECT_EQ(first->bounds().ToString(), second->bounds().ToString());
  EXPECT_EQ(gfx::Screen::GetNativeScreen()
                ->GetPrimaryDisplay()
                .work_area()
                .size()
                .ToString(),
            first->bounds().size().ToString());
  EXPECT_TRUE(first->IsVisible());
  EXPECT_TRUE(second->IsVisible());

  // Terminate overview mode. |first| should be hidden, since it's not visible
  // to the user anymore.
  WindowManager::Get()->ToggleOverview();
  ASSERT_FALSE(WindowManager::Get()->IsOverviewModeActive());
  EXPECT_FALSE(first->IsVisible());
  EXPECT_TRUE(second->IsVisible());
}

TEST_F(WindowManagerTest, OverviewToSplitViewMode) {
  test::WindowManagerImplTestApi wm_api;

  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> w1(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w2(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w3(CreateAndActivateWindow(&delegate));
  wm::ActivateWindow(w3.get());

  WindowManager::Get()->ToggleOverview();
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_TRUE(w3->IsVisible());

  // Go into split-view mode.
  WindowOverviewModeDelegate* overview_delegate = wm_api.wm();
  overview_delegate->OnSelectSplitViewWindow(w3.get(), NULL, w3.get());
  EXPECT_TRUE(w3->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_FALSE(w1->IsVisible());
}

TEST_F(WindowManagerTest, NewWindowFromOverview) {
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> w1(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w2(CreateAndActivateWindow(&delegate));

  WindowManager::Get()->ToggleOverview();
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());

  // Test that opening a new window exits overview mode. The new window could
  // have been opened by JavaScript or by the home card.
  scoped_ptr<aura::Window> w3(CreateAndActivateWindow(&delegate));

  ASSERT_FALSE(WindowManager::Get()->IsOverviewModeActive());
  EXPECT_TRUE(w3->IsVisible());
  EXPECT_TRUE(wm::IsActiveWindow(w3.get()));
  EXPECT_FALSE(w1->IsVisible());
  EXPECT_FALSE(w2->IsVisible());
}

TEST_F(WindowManagerTest, BezelGestureToSplitViewMode) {
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> first(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> second(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> third(CreateAndActivateWindow(&delegate));

  test::WindowManagerImplTestApi wm_api;

  // Test that going into split-view mode with two-finger gesture selects the
  // correct windows on left and right splits.
  ui::test::EventGenerator generator(root_window());
  const gfx::Point start_points[2] = {
      gfx::Point(2, 10), gfx::Point(4, 20),
  };
  const int kEventTimeSepration = 16;
  int x_middle = root_window()->bounds().width() / 2;
  generator.GestureMultiFingerScroll(
      2, start_points, kEventTimeSepration, 1, x_middle, 0);
  ASSERT_TRUE(wm_api.GetSplitViewController()->IsSplitViewModeActive());
  EXPECT_EQ(second.get(), wm_api.GetSplitViewController()->left_window());
  EXPECT_EQ(third.get(), wm_api.GetSplitViewController()->right_window());
  EXPECT_EQ(second->bounds().size().ToString(),
            third->bounds().size().ToString());
}

TEST_F(WindowManagerTest, BezelGestureToSwitchBetweenWindows) {
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> first(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> second(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> third(CreateAndActivateWindow(&delegate));
  first->Hide();
  second->Hide();

  test::WindowManagerImplTestApi wm_api;

  EXPECT_EQ(third.get(),
            wm_api.GetWindowListProvider()->GetWindowList().back());

  // Do a two-finger swipe from the left bezel.
  ui::test::EventGenerator generator(root_window());
  const gfx::Point left_bezel_points[2] = {
      gfx::Point(2, 10), gfx::Point(4, 20),
  };
  const int kEventTimeSeparation = 16;
  int width = root_window()->bounds().width();
  generator.GestureMultiFingerScroll(
      2, left_bezel_points, kEventTimeSeparation, 1, width, 0);
  EXPECT_TRUE(wm::IsActiveWindow(second.get()));
  EXPECT_EQ(second.get(),
            wm_api.GetWindowListProvider()->GetWindowList().back());

  // Do a two-finger swipe from the right bezel.
  const gfx::Point right_bezel_points[2] = {
      gfx::Point(width - 5, 10),
      gfx::Point(width - 10, 20)
  };
  generator.GestureMultiFingerScroll(
      2, right_bezel_points, kEventTimeSeparation, 1, -width, 0);
  EXPECT_TRUE(wm::IsActiveWindow(third.get()));
  EXPECT_EQ(third.get(),
            wm_api.GetWindowListProvider()->GetWindowList().back());
}

TEST_F(WindowManagerTest, TitleDragSwitchBetweenWindows) {
  aura::test::TestWindowDelegate delegate;
  delegate.set_window_component(HTCAPTION);
  scoped_ptr<aura::Window> first(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> second(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> third(CreateAndActivateWindow(&delegate));

  test::WindowManagerImplTestApi wm_api;

  EXPECT_EQ(third.get(),
            wm_api.GetWindowListProvider()->GetWindowList().back());

  // Do a title-swipe from the top to switch to the previous window.
  ui::test::EventGenerator generator(root_window());
  generator.GestureScrollSequence(gfx::Point(20, 10),
                                  gfx::Point(20, 400),
                                  base::TimeDelta::FromMilliseconds(20),
                                  5);
  EXPECT_TRUE(wm::IsActiveWindow(second.get()));
  EXPECT_EQ(second.get(),
            wm_api.GetWindowListProvider()->GetWindowList().back());
  EXPECT_TRUE(second->IsVisible());
  EXPECT_FALSE(third->IsVisible());

  // Performing the same gesture again will switch back to |third|.
  generator.GestureScrollSequence(gfx::Point(20, 10),
                                  gfx::Point(20, 400),
                                  base::TimeDelta::FromMilliseconds(20),
                                  5);
  EXPECT_TRUE(wm::IsActiveWindow(third.get()));
  EXPECT_EQ(third.get(),
            wm_api.GetWindowListProvider()->GetWindowList().back());
  EXPECT_FALSE(second->IsVisible());
  EXPECT_TRUE(third->IsVisible());

  // Perform a swipe that doesn't go enough to perform the window switch.
  generator.GestureScrollSequence(gfx::Point(20, 10),
                                  gfx::Point(20, 90),
                                  base::TimeDelta::FromMilliseconds(20),
                                  5);
  EXPECT_TRUE(wm::IsActiveWindow(third.get()));
  EXPECT_EQ(third.get(),
            wm_api.GetWindowListProvider()->GetWindowList().back());
  EXPECT_FALSE(second->IsVisible());
  EXPECT_TRUE(third->IsVisible());
}

TEST_F(WindowManagerTest, TitleDragSwitchBetweenWindowsInSplitViewMode) {
  aura::test::TestWindowDelegate delegate;
  delegate.set_window_component(HTCAPTION);
  scoped_ptr<aura::Window> first(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> second(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> third(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> fourth(CreateAndActivateWindow(&delegate));

  test::WindowManagerImplTestApi wm_api;

  // Test that going into split-view mode with two-finger gesture selects the
  // correct windows on left and right splits.
  ui::test::EventGenerator generator(root_window());
  const gfx::Point start_points[2] = {
      gfx::Point(2, 10), gfx::Point(4, 20),
  };
  const int kEventTimeSepration = 16;
  int x_middle = root_window()->bounds().width() / 2;
  generator.GestureMultiFingerScroll(
      2, start_points, kEventTimeSepration, 1, x_middle, 0);
  ASSERT_TRUE(wm_api.GetSplitViewController()->IsSplitViewModeActive());
  EXPECT_EQ(third.get(), wm_api.GetSplitViewController()->left_window());
  EXPECT_EQ(fourth.get(), wm_api.GetSplitViewController()->right_window());

  // Swipe the title of the left window. It should switch to |second|.
  generator.GestureScrollSequence(gfx::Point(20, 10),
                                  gfx::Point(20, 400),
                                  base::TimeDelta::FromMilliseconds(20),
                                  5);
  EXPECT_EQ(second.get(), wm_api.GetSplitViewController()->left_window());
  EXPECT_EQ(fourth.get(), wm_api.GetSplitViewController()->right_window());
  aura::Window::Windows windows =
      wm_api.GetWindowListProvider()->GetWindowList();
  ASSERT_EQ(4u, windows.size());
  EXPECT_EQ(second.get(), windows[3]);
  EXPECT_EQ(fourth.get(), windows[2]);

  // Swipe the title of the right window now. It should switch to |third|.
  generator.GestureScrollSequence(gfx::Point(x_middle + 20, 10),
                                  gfx::Point(x_middle + 20, 400),
                                  base::TimeDelta::FromMilliseconds(20),
                                  5);
  EXPECT_EQ(second.get(), wm_api.GetSplitViewController()->left_window());
  EXPECT_EQ(third.get(), wm_api.GetSplitViewController()->right_window());
  windows = wm_api.GetWindowListProvider()->GetWindowList();
  ASSERT_EQ(4u, windows.size());
  EXPECT_EQ(third.get(), windows[3]);
  EXPECT_EQ(second.get(), windows[2]);
}

TEST_F(WindowManagerTest, NewWindowBounds) {
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> first(CreateAndActivateWindow(&delegate));

  test::WindowManagerImplTestApi wm_api;
  // The window should have the same size as the container.
  const gfx::Size work_area =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().work_area().size();
  EXPECT_EQ(work_area.ToString(),
            first->bounds().size().ToString());
  EXPECT_TRUE(first->bounds().origin().IsOrigin());

  // A second window should have the same bounds as the first one.
  scoped_ptr<aura::Window> second(CreateAndActivateWindow(&delegate));
  EXPECT_EQ(first->bounds().ToString(), second->bounds().ToString());

  // Get into split view.
  wm_api.GetSplitViewController()->ActivateSplitMode(NULL, NULL, NULL);
  const gfx::Rect left_bounds =
      wm_api.GetSplitViewController()->left_window()->bounds();
  EXPECT_NE(work_area.ToString(),
            left_bounds.size().ToString());

  // A new window should replace the left window when in split view.
  scoped_ptr<aura::Window> third(CreateAndActivateWindow(&delegate));
  EXPECT_EQ(wm_api.GetSplitViewController()->left_window(), third.get());
  EXPECT_EQ(left_bounds.ToString(), third->bounds().ToString());
}

TEST_F(WindowManagerTest, SplitModeActivationByShortcut) {
  test::WindowManagerImplTestApi wm_api;

  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> w1(CreateAndActivateWindow(&delegate));

  // Splitview mode needs at least two windows.
  wm_api.wm()->ToggleSplitView();
  EXPECT_FALSE(wm_api.GetSplitViewController()->IsSplitViewModeActive());

  scoped_ptr<aura::Window> w2(CreateAndActivateWindow(&delegate));
  w2->Show();

  wm_api.wm()->ToggleSplitView();
  EXPECT_TRUE(wm_api.GetSplitViewController()->IsSplitViewModeActive());
  int width =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().work_area().width();

  EXPECT_EQ(w1->bounds().width(), w2->bounds().width());
  EXPECT_GE(width / 2, w1->bounds().width());

  // Toggle back to normal mode.
  wm_api.wm()->ToggleSplitView();
  EXPECT_FALSE(wm_api.GetSplitViewController()->IsSplitViewModeActive());

  EXPECT_EQ(width, w1->bounds().width());
  EXPECT_EQ(width, w2->bounds().width());
}

TEST_F(WindowManagerTest, OverviewModeFromSplitMode) {
  test::WindowManagerImplTestApi wm_api;

  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> w1(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w2(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w3(CreateAndActivateWindow(&delegate));

  // Get into split-view mode, and then turn on overview mode.
  wm_api.GetSplitViewController()->ActivateSplitMode(NULL, NULL, NULL);
  WindowManager::Get()->ToggleOverview();
  EXPECT_TRUE(wm_api.GetSplitViewController()->IsSplitViewModeActive());
  EXPECT_EQ(w3.get(), wm_api.GetSplitViewController()->left_window());
  EXPECT_EQ(w2.get(), wm_api.GetSplitViewController()->right_window());

  WindowOverviewModeDelegate* overview_delegate = wm_api.wm();
  overview_delegate->OnSelectWindow(w1.get());
  EXPECT_FALSE(wm_api.GetSplitViewController()->IsSplitViewModeActive());
  EXPECT_TRUE(w1->IsVisible());
  // Make sure the windows that were in split-view mode are hidden.
  EXPECT_FALSE(w2->IsVisible());
  EXPECT_FALSE(w3->IsVisible());
}

}  // namespace athena
