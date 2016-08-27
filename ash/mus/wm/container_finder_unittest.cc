// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/container_finder.h"

#include "ash/common/shell_window_ids.h"
#include "ash/common/wm_window.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/test/wm_test_base.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

using ContainerFinderTest = mus::WmTestBase;

TEST_F(ContainerFinderTest, GetContainerForWindow) {
  // Create a normal window in the default container.
  WmWindow* window =
      mus::WmWindowMus::Get(CreateTestWindow(gfx::Rect(1, 2, 3, 4)));

  // The window itself is not a container.
  EXPECT_EQ(kShellWindowId_Invalid, window->GetShellWindowId());

  // Container lookup finds the default container.
  WmWindow* container = wm::GetContainerForWindow(window);
  ASSERT_TRUE(container);
  EXPECT_EQ(kShellWindowId_DefaultContainer, container->GetShellWindowId());
}

}  // namespace ash
