// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_error_dialog.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "grit/ash_strings.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {
namespace {

class DisplayErrorDialogTest : public test::AshTestBase {
 protected:
  DisplayErrorDialogTest() {
  }

  virtual ~DisplayErrorDialogTest() {
  }

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    observer_.reset(new DisplayErrorObserver());
  }

  virtual void TearDown() OVERRIDE {
    if (observer_->dialog()) {
      views::Widget* widget =
          const_cast<DisplayErrorDialog*>(observer_->dialog())->GetWidget();
      widget->CloseNow();
    }
    observer_.reset();
    test::AshTestBase::TearDown();
  }

  DisplayErrorObserver* observer() { return observer_.get(); }

  const string16& GetMessageContents(const DisplayErrorDialog* dialog) {
    const views::Label* label = static_cast<const views::Label*>(
        static_cast<const views::View*>(dialog)->child_at(0));
    return label->text();
  }

 private:
  scoped_ptr<DisplayErrorObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(DisplayErrorDialogTest);
};

}

// The test cases in this file usually check if the showing dialog doesn't
// cause any crashes, and the code doesn't cause any memory leaks.
TEST_F(DisplayErrorDialogTest, Normal) {
  UpdateDisplay("200x200,300x300");
  DisplayErrorDialog* dialog =
      DisplayErrorDialog::ShowDialog(chromeos::STATE_DUAL_MIRROR);
  EXPECT_TRUE(dialog);
  EXPECT_TRUE(dialog->GetWidget()->IsVisible());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_DISPLAY_FAILURE_ON_MIRRORING),
            GetMessageContents(dialog));
  EXPECT_EQ(Shell::GetAllRootWindows()[1],
            dialog->GetWidget()->GetNativeView()->GetRootWindow());
}

TEST_F(DisplayErrorDialogTest, CallTwice) {
  UpdateDisplay("200x200,300x300");
  observer()->OnDisplayModeChangeFailed(chromeos::STATE_DUAL_MIRROR);
  const DisplayErrorDialog* dialog = observer()->dialog();
  EXPECT_TRUE(dialog);

  observer()->OnDisplayModeChangeFailed(chromeos::STATE_DUAL_MIRROR);
  EXPECT_EQ(dialog, observer()->dialog());
}

TEST_F(DisplayErrorDialogTest, CallWithDifferentState) {
  UpdateDisplay("200x200,300x300");
  observer()->OnDisplayModeChangeFailed(chromeos::STATE_DUAL_MIRROR);
  const DisplayErrorDialog* dialog = observer()->dialog();
  EXPECT_TRUE(dialog);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_DISPLAY_FAILURE_ON_MIRRORING),
            GetMessageContents(dialog));

  observer()->OnDisplayModeChangeFailed(chromeos::STATE_DUAL_EXTENDED);
  EXPECT_EQ(dialog, observer()->dialog());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_DISPLAY_FAILURE_ON_EXTENDED),
            GetMessageContents(dialog));
}

TEST_F(DisplayErrorDialogTest, SingleDisplay) {
  UpdateDisplay("200x200");
  DisplayErrorDialog* dialog =
      DisplayErrorDialog::ShowDialog(chromeos::STATE_DUAL_MIRROR);
  EXPECT_TRUE(dialog);
  EXPECT_TRUE(dialog->GetWidget()->IsVisible());
  EXPECT_EQ(Shell::GetInstance()->GetPrimaryRootWindow(),
            dialog->GetWidget()->GetNativeView()->GetRootWindow());
}

TEST_F(DisplayErrorDialogTest, DisplayDisconnected) {
  UpdateDisplay("200x200,300x300");
  observer()->OnDisplayModeChangeFailed(chromeos::STATE_DUAL_MIRROR);
  EXPECT_TRUE(observer()->dialog());

  UpdateDisplay("200x200");
  // Disconnection will close the dialog but we have to run all pending tasks
  // to make the effect of the close.
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(observer()->dialog());
}

}  // namespace internal
}  // namespace ash
