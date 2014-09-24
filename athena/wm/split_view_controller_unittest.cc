// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/split_view_controller.h"

#include "athena/screen/public/screen_manager.h"
#include "athena/test/athena_test_base.h"
#include "athena/wm/public/window_list_provider.h"
#include "athena/wm/test/window_manager_impl_test_api.h"
#include "base/memory/scoped_vector.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/wm/core/window_util.h"

namespace athena {

class SplitViewControllerTest : public test::AthenaTestBase {
 public:
  SplitViewControllerTest() {}
  virtual ~SplitViewControllerTest() {}

  // test::AthenaTestBase:
  virtual void SetUp() OVERRIDE {
    test::AthenaTestBase::SetUp();
    api_.reset(new test::WindowManagerImplTestApi);
  }

  virtual void TearDown() OVERRIDE {
    api_.reset();
    test::AthenaTestBase::TearDown();
  }

  // Returns the topmost window in z-order.
  const aura::Window* GetTopmostWindow() const {
    return *api_->GetWindowListProvider()->GetWindowList().rbegin();
  }

  // Returns the second topmost window in z-order.
  const aura::Window* GetSecondTopmostWindow() const {
    const aura::Window::Windows& list =
        api_->GetWindowListProvider()->GetWindowList();
    return *(list.rbegin() + 1);
  }

  // Returns whether only the split view windows are visible.
  bool OnlySplitViewWindowsVisible() const {
    SplitViewController* controller = api_->GetSplitViewController();
    DCHECK(controller->IsSplitViewModeActive());
    aura::Window::Windows list =
        api_->GetWindowListProvider()->GetWindowList();
    for (aura::Window::Windows::const_iterator it = list.begin();
         it != list.end(); ++it) {
      bool in_split_view = (*it == controller->left_window() ||
                            *it == controller->right_window());
      if (in_split_view != (*it)->IsVisible())
        return false;
    }
    return true;
  }

  bool IsSplitViewAllowed() const {
    return api_->GetSplitViewController()->CanActivateSplitViewMode();
  }

  test::WindowManagerImplTestApi* api() {
    return api_.get();
  }

 private:
  scoped_ptr<test::WindowManagerImplTestApi> api_;

  DISALLOW_COPY_AND_ASSIGN(SplitViewControllerTest);
};

// Tests that when split mode is activated, the windows on the left and right
// are selected correctly.
TEST_F(SplitViewControllerTest, SplitModeActivation) {
  aura::test::TestWindowDelegate delegate;
  ScopedVector<aura::Window> windows;
  const int kNumWindows = 6;
  for (size_t i = 0; i < kNumWindows; ++i) {
    scoped_ptr<aura::Window> window = CreateTestWindow(NULL, gfx::Rect());
    windows.push_back(window.release());
    windows[i]->Hide();
  }

  windows[kNumWindows - 1]->Show();
  wm::ActivateWindow(windows[kNumWindows - 1]);

  SplitViewController* controller = api()->GetSplitViewController();
  ASSERT_FALSE(controller->IsSplitViewModeActive());

  controller->ActivateSplitMode(NULL, NULL, NULL);
  ASSERT_TRUE(controller->IsSplitViewModeActive());
  // The last two windows should be on the left and right, respectively.
  EXPECT_EQ(windows[kNumWindows - 1], controller->left_window());
  EXPECT_EQ(windows[kNumWindows - 2], controller->right_window());
  EXPECT_EQ(windows[kNumWindows - 1], GetTopmostWindow());
  EXPECT_EQ(windows[kNumWindows - 2], GetSecondTopmostWindow());
  EXPECT_TRUE(OnlySplitViewWindowsVisible());

  // Select the window that is currently on the left for the right panel. The
  // windows should switch.
  controller->ActivateSplitMode(
      NULL, windows[kNumWindows - 1], windows[kNumWindows - 1]);
  EXPECT_EQ(windows[kNumWindows - 2], controller->left_window());
  EXPECT_EQ(windows[kNumWindows - 1], controller->right_window());
  EXPECT_EQ(windows[kNumWindows - 1], GetTopmostWindow());
  EXPECT_EQ(windows[kNumWindows - 2], GetSecondTopmostWindow());
  EXPECT_TRUE(OnlySplitViewWindowsVisible());

  controller->ActivateSplitMode(
      windows[kNumWindows - 1], NULL, windows[kNumWindows - 1]);
  EXPECT_EQ(windows[kNumWindows - 1], controller->left_window());
  EXPECT_EQ(windows[kNumWindows - 2], controller->right_window());
  EXPECT_EQ(windows[kNumWindows - 1], GetTopmostWindow());
  EXPECT_EQ(windows[kNumWindows - 2], GetSecondTopmostWindow());
  EXPECT_TRUE(OnlySplitViewWindowsVisible());

  // Select the same windows, but pass in a different window to activate.
  controller->ActivateSplitMode(windows[kNumWindows - 1],
                                windows[kNumWindows - 2],
                                windows[kNumWindows - 2]);
  EXPECT_EQ(windows[kNumWindows - 1], controller->left_window());
  EXPECT_EQ(windows[kNumWindows - 2], controller->right_window());
  EXPECT_EQ(windows[kNumWindows - 2], GetTopmostWindow());
  EXPECT_EQ(windows[kNumWindows - 1], GetSecondTopmostWindow());
  EXPECT_TRUE(OnlySplitViewWindowsVisible());

  // Select one of the windows behind the stacks for the right panel. The window
  // on the left should remain unchanged.
  controller->ActivateSplitMode(NULL, windows[0], windows[0]);
  EXPECT_EQ(windows[kNumWindows - 1], controller->left_window());
  EXPECT_EQ(windows[0], controller->right_window());
  EXPECT_EQ(windows[0], GetTopmostWindow());
  EXPECT_EQ(windows[kNumWindows - 1], GetSecondTopmostWindow());
  EXPECT_TRUE(OnlySplitViewWindowsVisible());

  controller->ActivateSplitMode(windows[1], NULL, NULL);
  EXPECT_EQ(windows[1], controller->left_window());
  EXPECT_EQ(windows[0], controller->right_window());
  EXPECT_EQ(windows[0], GetTopmostWindow());
  EXPECT_EQ(windows[1], GetSecondTopmostWindow());
  EXPECT_TRUE(OnlySplitViewWindowsVisible());

  controller->ActivateSplitMode(windows[4], windows[5], windows[5]);
  EXPECT_EQ(windows[4], controller->left_window());
  EXPECT_EQ(windows[5], controller->right_window());
  EXPECT_EQ(windows[5], GetTopmostWindow());
  EXPECT_EQ(windows[4], GetSecondTopmostWindow());
  EXPECT_TRUE(OnlySplitViewWindowsVisible());

  controller->ActivateSplitMode(windows[0], NULL, windows[0]);
  EXPECT_EQ(windows[0], controller->left_window());
  EXPECT_EQ(windows[5], controller->right_window());
  EXPECT_EQ(windows[0], GetTopmostWindow());
  EXPECT_EQ(windows[5], GetSecondTopmostWindow());
  EXPECT_TRUE(OnlySplitViewWindowsVisible());
}

TEST_F(SplitViewControllerTest, LandscapeOnly) {
  aura::test::TestWindowDelegate delegate;
  ScopedVector<aura::Window> windows;
  const int kNumWindows = 2;
  for (size_t i = 0; i < kNumWindows; ++i) {
    scoped_ptr<aura::Window> window = CreateTestWindow(NULL, gfx::Rect());
    window->Hide();
    windows.push_back(window.release());
  }
  windows[kNumWindows - 1]->Show();
  wm::ActivateWindow(windows[kNumWindows - 1]);

  ASSERT_EQ(gfx::Display::ROTATE_0,
            gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().rotation());

  SplitViewController* controller = api()->GetSplitViewController();
  ASSERT_TRUE(IsSplitViewAllowed());
  ASSERT_FALSE(controller->IsSplitViewModeActive());

  controller->ActivateSplitMode(NULL, NULL, NULL);
  ASSERT_TRUE(controller->IsSplitViewModeActive());

  // Screen rotation should be locked while in splitview.
  ScreenManager::Get()->SetRotation(gfx::Display::ROTATE_90);
  EXPECT_EQ(gfx::Display::ROTATE_0,
            gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().rotation());

  // Screen is rotated on exiting splitview.
  controller->DeactivateSplitMode();
  ASSERT_EQ(gfx::Display::ROTATE_90,
            gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().rotation());

  // Entering splitview should now be disabled now that the screen is in a
  // portrait orientation.
  EXPECT_FALSE(IsSplitViewAllowed());

  // Rotating back to 0 allows splitview again.
  ScreenManager::Get()->SetRotation(gfx::Display::ROTATE_0);
  EXPECT_TRUE(IsSplitViewAllowed());
}

}  // namespace athena
