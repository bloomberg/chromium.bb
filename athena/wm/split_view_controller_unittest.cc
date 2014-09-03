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

  bool IsSplitViewAllowed() const {
    return api_->GetSplitViewController()->CanScroll();
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
  }

  SplitViewController* controller = api()->GetSplitViewController();
  WindowListProvider* list_provider = api()->GetWindowListProvider();
  ASSERT_FALSE(controller->IsSplitViewModeActive());

  controller->ActivateSplitMode(NULL, NULL);
  ASSERT_TRUE(controller->IsSplitViewModeActive());
  // The last two windows should be on the left and right, respectively.
  EXPECT_EQ(windows[kNumWindows - 1], controller->left_window());
  EXPECT_EQ(windows[kNumWindows - 2], controller->right_window());

  // Select the window that is currently on the left for the right panel. The
  // windows should switch.
  controller->ActivateSplitMode(NULL, windows[kNumWindows - 1]);
  EXPECT_EQ(windows[kNumWindows - 2], controller->left_window());
  EXPECT_EQ(windows[kNumWindows - 1], controller->right_window());

  controller->ActivateSplitMode(windows[kNumWindows - 1], NULL);
  EXPECT_EQ(windows[kNumWindows - 1], controller->left_window());
  EXPECT_EQ(windows[kNumWindows - 2], controller->right_window());

  // Select one of the windows behind the stacks for the right panel. The window
  // on the left should remain unchanged.
  controller->ActivateSplitMode(NULL, windows[0]);
  EXPECT_EQ(windows[kNumWindows - 1], controller->left_window());
  EXPECT_EQ(windows[0], controller->right_window());
  EXPECT_EQ(windows[0], *list_provider->GetWindowList().rbegin());

  controller->ActivateSplitMode(windows[1], NULL);
  EXPECT_EQ(windows[1], controller->left_window());
  EXPECT_EQ(windows[0], controller->right_window());
  EXPECT_EQ(windows[1], *list_provider->GetWindowList().rbegin());

  controller->ActivateSplitMode(windows[4], windows[5]);
  EXPECT_EQ(windows[4], controller->left_window());
  EXPECT_EQ(windows[5], controller->right_window());
  EXPECT_EQ(windows[4], *list_provider->GetWindowList().rbegin());
  EXPECT_EQ(windows[5], *(list_provider->GetWindowList().rbegin() + 1));
}

TEST_F(SplitViewControllerTest, LandscapeOnly) {
  aura::test::TestWindowDelegate delegate;
  ScopedVector<aura::Window> windows;
  const int kNumWindows = 2;
  for (size_t i = 0; i < kNumWindows; ++i) {
    scoped_ptr<aura::Window> window = CreateTestWindow(NULL, gfx::Rect());
    windows.push_back(window.release());
  }
  ASSERT_EQ(gfx::Display::ROTATE_0,
            gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().rotation());

  SplitViewController* controller = api()->GetSplitViewController();
  ASSERT_TRUE(IsSplitViewAllowed());
  ASSERT_FALSE(controller->IsSplitViewModeActive());

  controller->ActivateSplitMode(NULL, NULL);
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
