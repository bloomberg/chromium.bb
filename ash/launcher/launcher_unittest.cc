// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher.h"

#include "ash/shell.h"
#include "ash/test/aura_shell_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

typedef aura_shell::test::AuraShellTestBase LauncherTest;

namespace aura_shell {

// Makes sure invoking SetStatusWidth on the launcher changes the size of the
// LauncherView.
TEST_F(LauncherTest, SetStatusWidth) {
  Launcher* launcher = Shell::GetInstance()->launcher();
  ASSERT_TRUE(launcher);
  views::View* launcher_view = launcher->widget()->GetContentsView();
  ASSERT_EQ(1, launcher_view->child_count());
  launcher_view = launcher_view->child_at(0);

  int total_width = launcher->widget()->GetWindowScreenBounds().width();
  ASSERT_GT(total_width, 0);
  launcher->SetStatusWidth(total_width / 2);
  EXPECT_EQ(total_width - total_width / 2, launcher_view->width());
}

}  // namespace aura_shell
