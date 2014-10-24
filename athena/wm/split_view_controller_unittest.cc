// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/split_view_controller.h"

#include "athena/screen/public/screen_manager.h"
#include "athena/test/base/athena_test_base.h"
#include "athena/test/base/test_windows.h"
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
  ~SplitViewControllerTest() override {}

  // test::AthenaTestBase:
  void SetUp() override {
    test::AthenaTestBase::SetUp();
    api_.reset(new test::WindowManagerImplTestApi);
  }

  void TearDown() override {
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

  void HandleScrollBegin(float delta) {
    api_->GetSplitViewController()->HandleScrollBegin(delta);
  }

  void HandleScrollUpdate(float delta) {
    api_->GetSplitViewController()->HandleScrollUpdate(delta);
  }

  void HandleScrollEnd(float velocity) {
    api_->GetSplitViewController()->HandleScrollEnd(velocity);
  }

  float GetMaxDistanceFromMiddleForTest() {
    return api_->GetSplitViewController()->GetMaxDistanceFromMiddleForTest();
  }

  float GetMinFlingVelocityForTest() {
    return api_->GetSplitViewController()->GetMinFlingVelocityForTest();
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
    scoped_ptr<aura::Window> window =
        test::CreateNormalWindow(nullptr, nullptr);
    windows.push_back(window.release());
    windows[i]->Hide();
  }

  windows[kNumWindows - 1]->Show();
  wm::ActivateWindow(windows[kNumWindows - 1]);

  SplitViewController* controller = api()->GetSplitViewController();
  ASSERT_FALSE(controller->IsSplitViewModeActive());

  controller->ActivateSplitMode(nullptr, nullptr, nullptr);
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
      nullptr, windows[kNumWindows - 1], windows[kNumWindows - 1]);
  EXPECT_EQ(windows[kNumWindows - 2], controller->left_window());
  EXPECT_EQ(windows[kNumWindows - 1], controller->right_window());
  EXPECT_EQ(windows[kNumWindows - 1], GetTopmostWindow());
  EXPECT_EQ(windows[kNumWindows - 2], GetSecondTopmostWindow());
  EXPECT_TRUE(OnlySplitViewWindowsVisible());

  controller->ActivateSplitMode(
      windows[kNumWindows - 1], nullptr, windows[kNumWindows - 1]);
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
  controller->ActivateSplitMode(nullptr, windows[0], windows[0]);
  EXPECT_EQ(windows[kNumWindows - 1], controller->left_window());
  EXPECT_EQ(windows[0], controller->right_window());
  EXPECT_EQ(windows[0], GetTopmostWindow());
  EXPECT_EQ(windows[kNumWindows - 1], GetSecondTopmostWindow());
  EXPECT_TRUE(OnlySplitViewWindowsVisible());

  controller->ActivateSplitMode(windows[1], nullptr, nullptr);
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

  controller->ActivateSplitMode(windows[0], nullptr, windows[0]);
  EXPECT_EQ(windows[0], controller->left_window());
  EXPECT_EQ(windows[5], controller->right_window());
  EXPECT_EQ(windows[0], GetTopmostWindow());
  EXPECT_EQ(windows[5], GetSecondTopmostWindow());
  EXPECT_TRUE(OnlySplitViewWindowsVisible());
}

// Helper function used to return the x-translation value of the layer
// containing |window|.
float XTranslationForWindow(aura::Window* window) {
  return window->layer()->transform().To2dTranslation().x();
}

// Tests that calls to the methods of DragHandleScrollDelegate will disengage
// split view mode under the correct circumstances.
TEST_F(SplitViewControllerTest, ScrollDragHandle) {
  aura::test::TestWindowDelegate delegate;
  ScopedVector<aura::Window> windows;
  const int kNumWindows = 2;
  for (size_t i = 0; i < kNumWindows; ++i) {
    scoped_ptr<aura::Window> window =
        test::CreateNormalWindow(nullptr, nullptr);
    windows.push_back(window.release());
    windows[i]->Hide();
  }

  SplitViewController* controller = api()->GetSplitViewController();
  ASSERT_FALSE(controller->IsSplitViewModeActive());

  aura::Window* left_window = windows[0];
  aura::Window* right_window = windows[1];
  left_window->Show();
  wm::ActivateWindow(left_window);

  // Activate split view.
  controller->ActivateSplitMode(left_window, right_window, left_window);
  ASSERT_TRUE(controller->IsSplitViewModeActive());
  EXPECT_TRUE(OnlySplitViewWindowsVisible());
  EXPECT_EQ(left_window, controller->left_window());
  EXPECT_EQ(right_window, controller->right_window());

  const float small_distance = GetMaxDistanceFromMiddleForTest() - 1.0f;
  const float large_distance = GetMaxDistanceFromMiddleForTest() + 1.0f;
  const float slow_velocity = GetMinFlingVelocityForTest() - 1.0f;
  const float fast_velocity = GetMinFlingVelocityForTest() + 1.0f;

  // Only scroll a small distance to the right, but not enough to be able to
  // disengage split view.
  EXPECT_EQ(0.0f, XTranslationForWindow(right_window));
  HandleScrollBegin(small_distance - 1.0f);
  EXPECT_EQ(small_distance - 1.0f, XTranslationForWindow(right_window));
  HandleScrollUpdate(small_distance);
  EXPECT_EQ(small_distance, XTranslationForWindow(right_window));
  HandleScrollEnd(0.0f);
  EXPECT_EQ(0.0f, XTranslationForWindow(right_window));
  ASSERT_TRUE(controller->IsSplitViewModeActive());
  EXPECT_EQ(left_window, controller->left_window());
  EXPECT_EQ(right_window, controller->right_window());

  // Scroll far enough to the right to be able to disengage split view. Split
  // view should be disengaged with the left window active.
  HandleScrollBegin(small_distance);
  HandleScrollUpdate(large_distance);
  HandleScrollEnd(0.0f);
  ASSERT_FALSE(controller->IsSplitViewModeActive());
  EXPECT_EQ(nullptr, controller->left_window());
  EXPECT_EQ(nullptr, controller->right_window());
  EXPECT_EQ(left_window, GetTopmostWindow());

  // Re-activate split view mode.
  controller->ActivateSplitMode(left_window, right_window, left_window);

  // Start scrolling a small distance and then fling, but not fast enough to
  // disengage split view (see kMinFlingVelocity). Split view mode should
  // remain engaged. Also verify that |right_window| is translated correctly.
  EXPECT_EQ(0.0f, XTranslationForWindow(right_window));
  HandleScrollBegin(-small_distance + 1.0f);
  EXPECT_EQ(-small_distance + 1.0f, XTranslationForWindow(right_window));
  HandleScrollUpdate(-small_distance);
  EXPECT_EQ(-small_distance, XTranslationForWindow(right_window));
  HandleScrollEnd(slow_velocity);
  EXPECT_EQ(0.0f, XTranslationForWindow(right_window));
  ASSERT_TRUE(controller->IsSplitViewModeActive());
  EXPECT_EQ(left_window, controller->left_window());
  EXPECT_EQ(right_window, controller->right_window());

  // Scroll far enough to the left to be able to disengage split view, then
  // fling to the right (but not faster than kMinFlingVelocity). Split view
  // should be disengaged with the right window active.
  HandleScrollBegin(-small_distance);
  HandleScrollUpdate(-large_distance);
  HandleScrollEnd(slow_velocity);
  ASSERT_FALSE(controller->IsSplitViewModeActive());
  EXPECT_EQ(nullptr, controller->left_window());
  EXPECT_EQ(nullptr, controller->right_window());
  EXPECT_EQ(right_window, GetTopmostWindow());

  // Re-activate split view mode.
  controller->ActivateSplitMode(left_window, right_window, left_window);

  // Scroll far enough to the left to be able to disengage split view, then
  // fling to the right, this time faster than kMinFlingVelocity). Split view
  // should be disengaged with the left window active.
  HandleScrollBegin(-small_distance);
  HandleScrollUpdate(-large_distance);
  HandleScrollEnd(fast_velocity);
  ASSERT_FALSE(controller->IsSplitViewModeActive());
  EXPECT_EQ(nullptr, controller->left_window());
  EXPECT_EQ(nullptr, controller->right_window());
  EXPECT_EQ(left_window, GetTopmostWindow());
}

TEST_F(SplitViewControllerTest, LandscapeOnly) {
  aura::test::TestWindowDelegate delegate;
  ScopedVector<aura::Window> windows;
  const int kNumWindows = 2;
  for (size_t i = 0; i < kNumWindows; ++i) {
    scoped_ptr<aura::Window> window =
        test::CreateNormalWindow(nullptr, nullptr);
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

  controller->ActivateSplitMode(nullptr, nullptr, nullptr);
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
