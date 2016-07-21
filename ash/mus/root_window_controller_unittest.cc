// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/test/wm_test_base.h"

namespace ash {

class RootWindowControllerTest : public mus::WmTestBase {
 public:
  RootWindowControllerTest() {}
  ~RootWindowControllerTest() override {}

  WmWindow* CreateFullscreenTestWindow() {
    return mus::WmWindowMus::Get(mus::WmTestBase::CreateFullscreenTestWindow());
  }

  WmWindow* GetPrimaryRootWindow() {
    return mus::WmWindowMus::Get(mus::WmTestBase::GetPrimaryRootWindow());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RootWindowControllerTest);
};

TEST_F(RootWindowControllerTest, CreateFullscreenWindow) {
  WmWindow* window = CreateFullscreenTestWindow();
  WmWindow* root_window = GetPrimaryRootWindow();
  EXPECT_EQ(root_window->GetBounds(), window->GetBounds());
}

}  // namespace ash
