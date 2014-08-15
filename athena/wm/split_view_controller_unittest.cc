// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/split_view_controller.h"

#include "athena/common/fill_layout_manager.h"
#include "athena/test/athena_test_base.h"
#include "athena/wm/public/window_list_provider.h"
#include "base/memory/scoped_vector.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"

namespace athena {

typedef test::AthenaTestBase SplitViewControllerTest;

class SimpleWindowListProvider : public WindowListProvider {
 public:
  explicit SimpleWindowListProvider(aura::Window* container)
      : container_(container) {}
  virtual ~SimpleWindowListProvider() {}

  // WindowListProvider:
  virtual aura::Window::Windows GetWindowList() const OVERRIDE {
    return container_->children();
  }

  virtual void MoveToFront(aura::Window* window) OVERRIDE {
    CHECK_EQ(container_, window->parent());
    container_->StackChildAtTop(window);
  }

 private:
  aura::Window* container_;

  DISALLOW_COPY_AND_ASSIGN(SimpleWindowListProvider);
};

// Tests that when split mode is activated, the windows on the left and right
// are selected correctly.
TEST_F(SplitViewControllerTest, SplitModeActivation) {
  aura::test::TestWindowDelegate delegate;
  ScopedVector<aura::Window> windows;
  const int kNumWindows = 6;
  for (size_t i = 0; i < kNumWindows; ++i) {
    aura::Window* window = new aura::Window(&delegate);
    window->Init(aura::WINDOW_LAYER_SOLID_COLOR);
    root_window()->AddChild(window);
    windows.push_back(window);
  }

  SimpleWindowListProvider list_provider(root_window());
  SplitViewController controller(root_window(), &list_provider, NULL);
  ASSERT_FALSE(controller.IsSplitViewModeActive());

  controller.ActivateSplitMode(NULL, NULL);
  ASSERT_TRUE(controller.IsSplitViewModeActive());
  // The last two windows should be on the left and right, respectively.
  EXPECT_EQ(windows[kNumWindows - 1], controller.left_window());
  EXPECT_EQ(windows[kNumWindows - 2], controller.right_window());

  // Select the window that is currently on the left for the right panel. The
  // windows should switch.
  controller.ActivateSplitMode(NULL, windows[kNumWindows - 1]);
  EXPECT_EQ(windows[kNumWindows - 2], controller.left_window());
  EXPECT_EQ(windows[kNumWindows - 1], controller.right_window());

  controller.ActivateSplitMode(windows[kNumWindows - 1], NULL);
  EXPECT_EQ(windows[kNumWindows - 1], controller.left_window());
  EXPECT_EQ(windows[kNumWindows - 2], controller.right_window());

  // Select one of the windows behind the stacks for the right panel. The window
  // on the left should remain unchanged.
  controller.ActivateSplitMode(NULL, windows[0]);
  EXPECT_EQ(windows[kNumWindows - 1], controller.left_window());
  EXPECT_EQ(windows[0], controller.right_window());
  EXPECT_EQ(windows[0], *list_provider.GetWindowList().rbegin());

  controller.ActivateSplitMode(windows[1], NULL);
  EXPECT_EQ(windows[1], controller.left_window());
  EXPECT_EQ(windows[0], controller.right_window());
  EXPECT_EQ(windows[1], *list_provider.GetWindowList().rbegin());

  controller.ActivateSplitMode(windows[4], windows[5]);
  EXPECT_EQ(windows[4], controller.left_window());
  EXPECT_EQ(windows[5], controller.right_window());
  EXPECT_EQ(windows[4], *list_provider.GetWindowList().rbegin());
  EXPECT_EQ(windows[5], *(list_provider.GetWindowList().rbegin() + 1));
}

}  // namespace athena
