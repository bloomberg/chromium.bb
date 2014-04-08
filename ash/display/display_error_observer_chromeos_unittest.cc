// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_error_observer_chromeos.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "grit/ash_strings.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

class DisplayErrorObserverTest : public test::AshTestBase {
 protected:
  DisplayErrorObserverTest() {
  }

  virtual ~DisplayErrorObserverTest() {
  }

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    observer_.reset(new DisplayErrorObserver());
  }

 protected:
  DisplayErrorObserver* observer() { return observer_.get(); }

  base::string16 GetMessageContents() {
    return observer_->GetTitleOfDisplayErrorNotificationForTest();
  }

 private:
  scoped_ptr<DisplayErrorObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(DisplayErrorObserverTest);
};

TEST_F(DisplayErrorObserverTest, Normal) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("200x200,300x300");
  observer()->OnDisplayModeChangeFailed(ui::MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_DISPLAY_FAILURE_ON_MIRRORING),
            GetMessageContents());
}

TEST_F(DisplayErrorObserverTest, CallTwice) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("200x200,300x300");
  observer()->OnDisplayModeChangeFailed(ui::MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  base::string16 message = GetMessageContents();
  EXPECT_FALSE(message.empty());

  observer()->OnDisplayModeChangeFailed(ui::MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  base::string16 message2 = GetMessageContents();
  EXPECT_FALSE(message2.empty());
  EXPECT_EQ(message, message2);
}

TEST_F(DisplayErrorObserverTest, CallWithDifferentState) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("200x200,300x300");
  observer()->OnDisplayModeChangeFailed(ui::MULTIPLE_DISPLAY_STATE_DUAL_MIRROR);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_DISPLAY_FAILURE_ON_MIRRORING),
            GetMessageContents());

  observer()->OnDisplayModeChangeFailed(
      ui::MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_DISPLAY_FAILURE_ON_NON_MIRRORING),
            GetMessageContents());
}

}  // namespace ash
