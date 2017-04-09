// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/container_finder.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/test/ash_test.h"
#include "ash/wm_window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

using ContainerFinderTest = AshTest;

TEST_F(ContainerFinderTest, GetContainerForWindow) {
  // Create a normal widget in the default container.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(gfx::Rect(1, 2, 3, 4));
  WmWindow* window = WmWindow::Get(widget->GetNativeWindow());

  // The window itself is not a container.
  EXPECT_EQ(kShellWindowId_Invalid, window->GetShellWindowId());

  // Container lookup finds the default container.
  WmWindow* container = wm::GetContainerForWindow(window);
  ASSERT_TRUE(container);
  EXPECT_EQ(kShellWindowId_DefaultContainer, container->GetShellWindowId());
}

}  // namespace ash
