// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_util.h"

#include "ash/screen_ash.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/window.h"

namespace ash {

typedef test::AshTestBase WindowUtilTest;

TEST_F(WindowUtilTest, CenterWindow) {
  UpdateDisplay("500x400, 600x400");
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(12, 20, 100, 100)));
  wm::CenterWindow(window.get());
  EXPECT_EQ("200,126 100x100", window->bounds().ToString());
  EXPECT_EQ("200,126 100x100", window->GetBoundsInScreen().ToString());
  window->SetBoundsInScreen(gfx::Rect(600, 0, 100, 100),
                            ScreenAsh::GetSecondaryDisplay());
  wm::CenterWindow(window.get());
  EXPECT_EQ("250,126 100x100", window->bounds().ToString());
  EXPECT_EQ("750,126 100x100", window->GetBoundsInScreen().ToString());
}

}  // namespace ash
