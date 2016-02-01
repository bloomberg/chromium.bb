// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_util.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"

namespace ash {

typedef test::AshTestBase DisplayUtilTest;

TEST_F(DisplayUtilTest, RotatedDisplay) {
  if (!SupportsMultipleDisplays())
    return;
  {
    UpdateDisplay("10+10-500x400,600+10-1000x600/r");
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    AshWindowTreeHost* host0 =
        GetRootWindowController(root_windows[0])->ash_host();
    AshWindowTreeHost* host1 =
        GetRootWindowController(root_windows[1])->ash_host();
    gfx::Rect rect0 = GetNativeEdgeBounds(host0, gfx::Rect(499, 10, 1, 300));
    gfx::Rect rect1 = GetNativeEdgeBounds(host1, gfx::Rect(500, 10, 1, 300));
    EXPECT_EQ("509,20 1x300", rect0.ToString());
    EXPECT_EQ("1289,10 300x1", rect1.ToString());
  }
  {
    UpdateDisplay("10+10-500x400,600+10-1000x600/l");
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    AshWindowTreeHost* host0 =
        GetRootWindowController(root_windows[0])->ash_host();
    AshWindowTreeHost* host1 =
        GetRootWindowController(root_windows[1])->ash_host();
    gfx::Rect rect0 = GetNativeEdgeBounds(host0, gfx::Rect(499, 10, 1, 300));
    gfx::Rect rect1 = GetNativeEdgeBounds(host1, gfx::Rect(500, 10, 1, 300));
    EXPECT_EQ("509,20 1x300", rect0.ToString());
    EXPECT_EQ("610,609 300x1", rect1.ToString());
  }
  {
    UpdateDisplay("10+10-500x400,600+10-1000x600/u");
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    AshWindowTreeHost* host0 =
        GetRootWindowController(root_windows[0])->ash_host();
    AshWindowTreeHost* host1 =
        GetRootWindowController(root_windows[1])->ash_host();
    gfx::Rect rect0 = GetNativeEdgeBounds(host0, gfx::Rect(499, 10, 1, 300));
    gfx::Rect rect1 = GetNativeEdgeBounds(host1, gfx::Rect(500, 10, 1, 300));
    EXPECT_EQ("509,20 1x300", rect0.ToString());
    EXPECT_EQ("1599,299 1x300", rect1.ToString());
  }

  {
    UpdateDisplay("10+10-500x400/r,600+10-1000x600");
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    AshWindowTreeHost* host0 =
        GetRootWindowController(root_windows[0])->ash_host();
    AshWindowTreeHost* host1 =
        GetRootWindowController(root_windows[1])->ash_host();
    gfx::Rect rect0 = GetNativeEdgeBounds(host0, gfx::Rect(399, 10, 1, 300));
    gfx::Rect rect1 = GetNativeEdgeBounds(host1, gfx::Rect(400, 10, 1, 300));
    EXPECT_EQ("199,409 300x1", rect0.ToString());
    EXPECT_EQ("600,20 1x300", rect1.ToString());
  }
  {
    UpdateDisplay("10+10-500x400/l,600+10-1000x600");
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    AshWindowTreeHost* host0 =
        GetRootWindowController(root_windows[0])->ash_host();
    AshWindowTreeHost* host1 =
        GetRootWindowController(root_windows[1])->ash_host();
    gfx::Rect rect0 = GetNativeEdgeBounds(host0, gfx::Rect(499, 10, 1, 300));
    gfx::Rect rect1 = GetNativeEdgeBounds(host1, gfx::Rect(500, 10, 1, 300));
    EXPECT_EQ("20,10 300x1", rect0.ToString());
    EXPECT_EQ("600,20 1x300", rect1.ToString());
  }
  {
    UpdateDisplay("10+10-500x400/u,600+10-1000x600");
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    AshWindowTreeHost* host0 =
        GetRootWindowController(root_windows[0])->ash_host();
    AshWindowTreeHost* host1 =
        GetRootWindowController(root_windows[1])->ash_host();
    gfx::Rect rect0 = GetNativeEdgeBounds(host0, gfx::Rect(499, 10, 1, 300));
    gfx::Rect rect1 = GetNativeEdgeBounds(host1, gfx::Rect(500, 10, 1, 300));
    EXPECT_EQ("10,99 1x300", rect0.ToString());
    EXPECT_EQ("600,20 1x300", rect1.ToString());
  }
}

TEST_F(DisplayUtilTest, CreateDisplayIdPair) {
  DisplayIdList list = CreateDisplayIdList(10, 1);
  EXPECT_EQ(1, list[0]);
  EXPECT_EQ(10, list[1]);
  list = CreateDisplayIdList(10, 100);
  EXPECT_EQ(10, list[0]);
  EXPECT_EQ(100, list[1]);
  {
    test::ScopedSetInternalDisplayId set_internal(100);
    list = CreateDisplayIdList(10, 100);
    EXPECT_EQ(100, list[0]);
    EXPECT_EQ(10, list[1]);

    list = CreateDisplayIdList(100, 10);
    EXPECT_EQ(100, list[0]);
    EXPECT_EQ(10, list[1]);
  }

  {
    test::ScopedSetInternalDisplayId set_internal(10);
    list = CreateDisplayIdList(10, 100);
    EXPECT_EQ(10, list[0]);
    EXPECT_EQ(100, list[1]);

    list = CreateDisplayIdList(100, 10);
    EXPECT_EQ(10, list[0]);
    EXPECT_EQ(100, list[1]);
  }
}

}  // namespace
