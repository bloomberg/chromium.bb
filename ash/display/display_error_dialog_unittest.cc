// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_error_dialog.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

typedef test::AshTestBase DisplayErrorDialogTest;

// The test cases in this file usually check if the showing dialog doesn't
// cause any crashes, and the code doesn't cause any memory leaks.
TEST_F(DisplayErrorDialogTest, Normal) {
  UpdateDisplay("200x200,300x300");
  DisplayErrorDialog::ShowDialog();
  DisplayErrorDialog* dialog = DisplayErrorDialog::GetInstanceForTest();
  EXPECT_TRUE(dialog);
  EXPECT_TRUE(dialog->GetWidget()->IsVisible());
  EXPECT_EQ(Shell::GetAllRootWindows()[1],
            dialog->GetWidget()->GetNativeView()->GetRootWindow());
}

TEST_F(DisplayErrorDialogTest, CallTwice) {
  UpdateDisplay("200x200,300x300");
  DisplayErrorDialog::ShowDialog();
  DisplayErrorDialog* dialog = DisplayErrorDialog::GetInstanceForTest();
  EXPECT_TRUE(dialog);
  DisplayErrorDialog::ShowDialog();
  EXPECT_EQ(dialog, DisplayErrorDialog::GetInstanceForTest());
}

TEST_F(DisplayErrorDialogTest, SingleDisplay) {
  UpdateDisplay("200x200");
  DisplayErrorDialog::ShowDialog();
  DisplayErrorDialog* dialog = DisplayErrorDialog::GetInstanceForTest();
  EXPECT_TRUE(dialog);
  EXPECT_TRUE(dialog->GetWidget()->IsVisible());
  EXPECT_EQ(Shell::GetInstance()->GetPrimaryRootWindow(),
            dialog->GetWidget()->GetNativeView()->GetRootWindow());
}

TEST_F(DisplayErrorDialogTest, DisplayDisconnected) {
  UpdateDisplay("200x200,300x300");
  DisplayErrorDialog::ShowDialog();
  DisplayErrorDialog* dialog = DisplayErrorDialog::GetInstanceForTest();
  EXPECT_TRUE(dialog);

  UpdateDisplay("200x200");
  // Disconnection will close the dialog but we have to run all pending tasks
  // to make the effect of the close.
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(DisplayErrorDialog::GetInstanceForTest());
}

}  // namespace internal
}  // namespace ash
