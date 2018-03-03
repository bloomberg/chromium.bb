// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/chromevox_layout_manager.h"

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

ChromeVoxLayoutManager* GetLayoutManager() {
  aura::Window* container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_ChromeVoxContainer);
  return static_cast<ChromeVoxLayoutManager*>(container->layout_manager());
}

using ChromeVoxLayoutManagerTest = AshTestBase;

TEST_F(ChromeVoxLayoutManagerTest, Basics) {
  ChromeVoxLayoutManager* layout_manager = GetLayoutManager();
  ASSERT_TRUE(layout_manager);

  // The layout manager doesn't track anything at startup.
  EXPECT_FALSE(layout_manager->chromevox_window_for_test());

  // Simulate chrome creating the ChromeVox widget. The layout manager starts
  // managing it.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(nullptr, kShellWindowId_ChromeVoxContainer);
  EXPECT_EQ(widget->GetNativeWindow(),
            layout_manager->chromevox_window_for_test());

  // The layout manager doesn't track anything after the widget closes.
  widget.reset();
  EXPECT_FALSE(layout_manager->chromevox_window_for_test());
}

TEST_F(ChromeVoxLayoutManagerTest, Shutdown) {
  ChromeVoxLayoutManager* layout_manager = GetLayoutManager();
  ASSERT_TRUE(layout_manager);

  // Simulate chrome creating the ChromeVox widget. The layout manager starts
  // managing it.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(nullptr, kShellWindowId_ChromeVoxContainer);
  EXPECT_EQ(widget->GetNativeWindow(),
            layout_manager->chromevox_window_for_test());

  // Don't close the window.
  widget.release();

  // Ash should not crash if the window is still open at shutdown.
}

// TODO: Test the following:
// - Initial ChromeVox window bounds.
// - Display work area updates.
// - Bounds and work area updates during fullscreen.

}  // namespace
}  // namespace ash
