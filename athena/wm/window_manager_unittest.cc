// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/public/window_manager.h"

#include "athena/screen/public/screen_manager.h"
#include "athena/test/base/athena_test_base.h"
#include "athena/test/base/test_windows.h"
#include "athena/wm/public/window_list_provider.h"
#include "athena/wm/split_view_controller.h"
#include "athena/wm/test/window_manager_impl_test_api.h"
#include "athena/wm/window_manager_impl.h"
#include "ui/aura/client/aura_constants.h"
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
  ~WindowManagerTest() override {}

  scoped_ptr<aura::Window> CreateAndActivateWindow(
      aura::WindowDelegate* delegate) {
    scoped_ptr<aura::Window> window(
        test::CreateNormalWindow(delegate, nullptr));
    window->Show();
    wm::ActivateWindow(window.get());
    window->SetProperty(aura::client::kCanMaximizeKey, true);
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
  WindowManager::Get()->EnterOverview();
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
  WindowManager::Get()->ExitOverview();
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

  WindowManager::Get()->EnterOverview();
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_TRUE(w3->IsVisible());

  // Go into split-view mode.
  WindowOverviewModeDelegate* overview_delegate = wm_api.wm();
  overview_delegate->OnSelectSplitViewWindow(w3.get(), nullptr, w3.get());
  EXPECT_TRUE(w3->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_FALSE(w1->IsVisible());
}

TEST_F(WindowManagerTest, OnSelectWindow) {
  test::WindowManagerImplTestApi wm_api;
  aura::test::TestWindowDelegate delegate1;
  aura::test::TestWindowDelegate delegate2;
  aura::test::TestWindowDelegate delegate3;

  // (w1): A window that sets a maximum size
  delegate1.set_maximum_size(gfx::Size(300, 200));
  scoped_ptr<aura::Window> w1(CreateAndActivateWindow(&delegate1));
  w1->SetBounds(gfx::Rect(0, 0, 300, 200));

  // (w2): A window that doesn't set a max size and is maximizable
  scoped_ptr<aura::Window> w2(CreateAndActivateWindow(&delegate2));
  w2->SetBounds(gfx::Rect(0, 0, 300, 200));
  w2->SetProperty(aura::client::kCanMaximizeKey, true);

  // (w3): A window that doesn't set a max size but is NOT maximizable
  scoped_ptr<aura::Window> w3(CreateAndActivateWindow(&delegate3));
  w3->SetBounds(gfx::Rect(0, 0, 300, 200));
  w3->SetProperty(aura::client::kCanMaximizeKey, false);

  const gfx::Size old_bounds1 = w1->GetTargetBounds().size();
  const gfx::Size old_bounds2 = w2->GetTargetBounds().size();
  const gfx::Size old_bounds3 = w3->GetTargetBounds().size();
  const gfx::Size work_area =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().work_area().size();

  // Select w1, which has a max size, in the overview mode and make sure it's
  // not maximized.
  WindowManager::Get()->EnterOverview();
  WindowOverviewModeDelegate* overview_delegate = wm_api.wm();
  overview_delegate->OnSelectWindow(w1.get());
  const gfx::Size new_bounds1 = w1->GetTargetBounds().size();
  EXPECT_EQ(new_bounds1.ToString(), old_bounds1.ToString());
  EXPECT_NE(work_area.ToString(), new_bounds1.ToString());

  // Select w2, which has no max size & can be maximized, in the overview
  // mode and make sure it's actually maximized.
  WindowManager::Get()->EnterOverview();
  overview_delegate->OnSelectWindow(w2.get());
  const gfx::Size new_bounds2 = w2->GetTargetBounds().size();
  EXPECT_NE(new_bounds2.ToString(), old_bounds2.ToString());
  EXPECT_EQ(work_area.ToString(), new_bounds2.ToString());

  // Select w3, which has no max size & cannot be maximized, in the overview
  // mode and make sure it's not maximized.
  WindowManager::Get()->EnterOverview();
  overview_delegate->OnSelectWindow(w3.get());
  const gfx::Size new_bounds3 = w3->GetTargetBounds().size();
  EXPECT_EQ(new_bounds3.ToString(), old_bounds3.ToString());
  EXPECT_NE(work_area.ToString(), new_bounds3.ToString());
}

TEST_F(WindowManagerTest, NewWindowFromOverview) {
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> w1(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w2(CreateAndActivateWindow(&delegate));

  WindowManager::Get()->EnterOverview();
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

// Verifies that the correct windows are replaced and the ordering of
// the window list changes correctly when the window titlebars are dragged
// in split view mode.
TEST_F(WindowManagerTest, ReplaceWindowsInSplitViewMode) {
  aura::test::TestWindowDelegate delegate;
  delegate.set_window_component(HTCAPTION);
  scoped_ptr<aura::Window> first(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> second(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> third(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> fourth(CreateAndActivateWindow(&delegate));

  test::WindowManagerImplTestApi wm_api;
  wm_api.wm()->ToggleSplitView();
  ASSERT_TRUE(wm_api.GetSplitViewController()->IsSplitViewModeActive());

  // Verify that the left and right windows have been set to the
  // two windows at the top of the window list.
  aura::Window::Windows windows =
      wm_api.GetWindowListProvider()->GetWindowList();
  ASSERT_EQ(4u, windows.size());
  EXPECT_EQ(first.get(), windows[0]);
  EXPECT_EQ(second.get(), windows[1]);
  EXPECT_EQ(third.get(), windows[2]);
  EXPECT_EQ(fourth.get(), windows[3]);
  EXPECT_EQ(fourth.get(), wm_api.GetSplitViewController()->left_window());
  EXPECT_EQ(third.get(), wm_api.GetSplitViewController()->right_window());

  // Swiping the title of the left window should change it from |fourth|
  // to |second|. The right window should not change.
  ui::test::EventGenerator generator(root_window());
  generator.GestureScrollSequence(gfx::Point(20, 10),
                                  gfx::Point(20, 400),
                                  base::TimeDelta::FromMilliseconds(20),
                                  5);
  EXPECT_EQ(second.get(), wm_api.GetSplitViewController()->left_window());
  EXPECT_EQ(third.get(), wm_api.GetSplitViewController()->right_window());

  // |second| should be placed at the front of the list. The replaced
  // window (|fourth|) should be stacked behind the non-replaced window
  // (|third|).
  windows = wm_api.GetWindowListProvider()->GetWindowList();
  ASSERT_EQ(4u, windows.size());
  EXPECT_EQ(first.get(), windows[0]);
  EXPECT_EQ(fourth.get(), windows[1]);
  EXPECT_EQ(third.get(), windows[2]);
  EXPECT_EQ(second.get(), windows[3]);

  // Swiping the title of the right window should change it from |third|
  // to |fourth|. The left window should not change.
  int x_middle = root_window()->bounds().width() / 2;
  generator.GestureScrollSequence(gfx::Point(x_middle + 20, 10),
                                  gfx::Point(x_middle + 20, 400),
                                  base::TimeDelta::FromMilliseconds(20),
                                  5);
  EXPECT_EQ(second.get(), wm_api.GetSplitViewController()->left_window());
  EXPECT_EQ(fourth.get(), wm_api.GetSplitViewController()->right_window());

  // |fourth| should be placed at the front of the list. The replaced
  // window (|third|) should be stacked behind the non-replaced window
  // (|second|).
  windows = wm_api.GetWindowListProvider()->GetWindowList();
  ASSERT_EQ(4u, windows.size());
  EXPECT_EQ(first.get(), windows[0]);
  EXPECT_EQ(third.get(), windows[1]);
  EXPECT_EQ(second.get(), windows[2]);
  EXPECT_EQ(fourth.get(), windows[3]);
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
  wm_api.GetSplitViewController()->ActivateSplitMode(nullptr, nullptr, nullptr);
  const gfx::Rect left_bounds =
      wm_api.GetSplitViewController()->left_window()->bounds();
  EXPECT_NE(work_area.ToString(),
            left_bounds.size().ToString());

  // A new window should replace the left window when in split view.
  scoped_ptr<aura::Window> third(CreateAndActivateWindow(&delegate));
  EXPECT_EQ(wm_api.GetSplitViewController()->left_window(), third.get());
  EXPECT_EQ(left_bounds.ToString(), third->bounds().ToString());
}

TEST_F(WindowManagerTest, OverviewModeAccelerators) {
  ui::test::EventGenerator generator(root_window());

  // F6 toggles overview mode
  ASSERT_FALSE(WindowManager::Get()->IsOverviewModeActive());
  generator.PressKey(ui::VKEY_F6, ui::EF_NONE);
  EXPECT_TRUE(WindowManager::Get()->IsOverviewModeActive());
  generator.PressKey(ui::VKEY_F6, ui::EF_NONE);
  EXPECT_FALSE(WindowManager::Get()->IsOverviewModeActive());

  // ESC exits overview mode
  generator.PressKey(ui::VKEY_F6, ui::EF_NONE);
  EXPECT_TRUE(WindowManager::Get()->IsOverviewModeActive());
  generator.PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  EXPECT_FALSE(WindowManager::Get()->IsOverviewModeActive());
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

  // w2 is the top window, it should be resized to max width and must be visible
  // w1 should be hidden.
  EXPECT_EQ(width, w2->bounds().width());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_FALSE(w1->IsVisible());

  // Expect w1 to be visible and maximized when activated.
  // TODO(oshima): To change to wm::ActivateWindow once the correct window state
  // is implemented.
  w1->Show();
  EXPECT_EQ(width, w1->bounds().width());
  EXPECT_TRUE(w1->IsVisible());
}

TEST_F(WindowManagerTest, OverviewModeFromSplitMode) {
  test::WindowManagerImplTestApi wm_api;

  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> w1(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w2(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w3(CreateAndActivateWindow(&delegate));

  // Get into split-view mode, and then turn on overview mode.
  wm_api.GetSplitViewController()->ActivateSplitMode(nullptr, nullptr, nullptr);
  WindowManager::Get()->EnterOverview();
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

// Clicking a background in overview should not crash.
TEST_F(WindowManagerTest, ClickBackgroundInOverview) {
  test::WindowManagerImplTestApi wm_api;

  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> w1(CreateAndActivateWindow(&delegate));
  WindowManager::Get()->EnterOverview();

  ui::test::EventGenerator generator(root_window());
  generator.MoveMouseTo(1, 1);
  generator.ClickLeftButton();
}

TEST_F(WindowManagerTest, CloseWindow) {
  test::WindowManagerImplTestApi wm_api;

  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> w1(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w2(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w3(CreateAndActivateWindow(&delegate));

  EXPECT_EQ(w3.get(),
            wm_api.GetWindowListProvider()->GetWindowList().back());
  EXPECT_TRUE(w3->IsVisible());
  EXPECT_TRUE(wm::IsActiveWindow(w3.get()));

  // Closes the front window (w3).
  w3.reset();
  EXPECT_EQ(w2.get(),
            wm_api.GetWindowListProvider()->GetWindowList().back());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));

  // Closes a non-front window (w1).
  w1.reset();
  EXPECT_EQ(w2.get(),
            wm_api.GetWindowListProvider()->GetWindowList().back());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));

  // Closes w2.
  w2.reset();
  EXPECT_TRUE(wm_api.GetWindowListProvider()->GetWindowList().empty());
}

TEST_F(WindowManagerTest, CloseWindowInSplitMode) {
  test::WindowManagerImplTestApi wm_api;

  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> w1(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w2(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w3(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w4(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w5(CreateAndActivateWindow(&delegate));

  // Get into split-view mode.
  wm_api.GetSplitViewController()->ActivateSplitMode(NULL, NULL, NULL);
  EXPECT_TRUE(wm_api.GetSplitViewController()->IsSplitViewModeActive());
  EXPECT_EQ(w5.get(), wm_api.GetSplitViewController()->left_window());
  EXPECT_EQ(w4.get(), wm_api.GetSplitViewController()->right_window());
  EXPECT_TRUE(w5->IsVisible());
  EXPECT_TRUE(w4->IsVisible());
  EXPECT_TRUE(wm::IsActiveWindow(w5.get()));

  // Closes the left window (w5).
  w5.reset();

  EXPECT_TRUE(wm_api.GetSplitViewController()->IsSplitViewModeActive());
  EXPECT_EQ(w3.get(), wm_api.GetSplitViewController()->left_window());
  EXPECT_EQ(w4.get(), wm_api.GetSplitViewController()->right_window());
  EXPECT_TRUE(w4->IsVisible());
  EXPECT_TRUE(w3->IsVisible());
  EXPECT_TRUE(wm::IsActiveWindow(w4.get()));

  // Closes the right window (w4).
  w4.reset();

  EXPECT_TRUE(wm_api.GetSplitViewController()->IsSplitViewModeActive());
  EXPECT_EQ(w3.get(), wm_api.GetSplitViewController()->left_window());
  EXPECT_EQ(w2.get(), wm_api.GetSplitViewController()->right_window());
  EXPECT_TRUE(w3->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_TRUE(wm::IsActiveWindow(w3.get()));

  // Closes a non-visible window (w1).
  w1.reset();

  EXPECT_TRUE(wm_api.GetSplitViewController()->IsSplitViewModeActive());
  EXPECT_EQ(w3.get(), wm_api.GetSplitViewController()->left_window());
  EXPECT_EQ(w2.get(), wm_api.GetSplitViewController()->right_window());
  EXPECT_TRUE(w3->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_TRUE(wm::IsActiveWindow(w3.get()));

  // Closes a non-visible window (w1).
  w3.reset();

  EXPECT_FALSE(wm_api.GetSplitViewController()->IsSplitViewModeActive());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
}

TEST_F(WindowManagerTest, CloseWindowFromOverview) {
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> w1(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w2(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w3(CreateAndActivateWindow(&delegate));
  EXPECT_FALSE(wm::IsActiveWindow(w1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(w2.get()));
  EXPECT_TRUE(wm::IsActiveWindow(w3.get()));

  WindowManager::Get()->EnterOverview();

  ASSERT_TRUE(WindowManager::Get()->IsOverviewModeActive());
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_TRUE(w3->IsVisible());
  EXPECT_FALSE(wm::IsActiveWindow(w1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(w2.get()));
  EXPECT_FALSE(wm::IsActiveWindow(w3.get()));

  w3.reset();

  ASSERT_TRUE(WindowManager::Get()->IsOverviewModeActive());
  EXPECT_FALSE(wm::IsActiveWindow(w1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(w2.get()));
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());

  w1.reset();

  ASSERT_TRUE(WindowManager::Get()->IsOverviewModeActive());
  EXPECT_FALSE(wm::IsActiveWindow(w2.get()));
  EXPECT_TRUE(w2->IsVisible());
}

TEST_F(WindowManagerTest, CloseWindowInSplitModeFromOverview) {
  test::WindowManagerImplTestApi wm_api;

  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> w1(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w2(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w3(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w4(CreateAndActivateWindow(&delegate));
  scoped_ptr<aura::Window> w5(CreateAndActivateWindow(&delegate));

  // Get into split-view mode.
  wm_api.GetSplitViewController()->ActivateSplitMode(NULL, NULL, NULL);
  WindowManager::Get()->EnterOverview();

  ASSERT_TRUE(WindowManager::Get()->IsOverviewModeActive());
  EXPECT_TRUE(wm_api.GetSplitViewController()->IsSplitViewModeActive());
  EXPECT_EQ(w5.get(), wm_api.GetSplitViewController()->left_window());
  EXPECT_EQ(w4.get(), wm_api.GetSplitViewController()->right_window());
  EXPECT_TRUE(w5->IsVisible());
  EXPECT_TRUE(w4->IsVisible());

  // Closes the left window (w5).
  w5.reset();

  ASSERT_TRUE(WindowManager::Get()->IsOverviewModeActive());
  EXPECT_TRUE(wm_api.GetSplitViewController()->IsSplitViewModeActive());
  EXPECT_EQ(w3.get(), wm_api.GetSplitViewController()->left_window());
  EXPECT_EQ(w4.get(), wm_api.GetSplitViewController()->right_window());
  EXPECT_TRUE(w4->IsVisible());
  EXPECT_TRUE(w3->IsVisible());

  // Closes the right window (w4).
  w4.reset();

  ASSERT_TRUE(WindowManager::Get()->IsOverviewModeActive());
  EXPECT_TRUE(wm_api.GetSplitViewController()->IsSplitViewModeActive());
  EXPECT_EQ(w3.get(), wm_api.GetSplitViewController()->left_window());
  EXPECT_EQ(w2.get(), wm_api.GetSplitViewController()->right_window());
  EXPECT_TRUE(w3->IsVisible());
  EXPECT_TRUE(w2->IsVisible());

  // Closes a non-visible window (w1).
  w1.reset();

  ASSERT_TRUE(WindowManager::Get()->IsOverviewModeActive());
  EXPECT_TRUE(wm_api.GetSplitViewController()->IsSplitViewModeActive());
  EXPECT_EQ(w3.get(), wm_api.GetSplitViewController()->left_window());
  EXPECT_EQ(w2.get(), wm_api.GetSplitViewController()->right_window());
  EXPECT_TRUE(w3->IsVisible());
  EXPECT_TRUE(w2->IsVisible());

  // Closes a non-visible window (w1).
  w3.reset();

  ASSERT_TRUE(WindowManager::Get()->IsOverviewModeActive());
  EXPECT_FALSE(wm_api.GetSplitViewController()->IsSplitViewModeActive());
  EXPECT_TRUE(w2->IsVisible());
}

}  // namespace athena
