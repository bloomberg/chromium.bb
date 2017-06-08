// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/browser_modal_dialog.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

class MockBrowserModalDialog : public BrowserModalDialog {
 public:
  MockBrowserModalDialog() {
    BrowserModalDialogList::GetInstance()->AddDialog(this);
  }
  ~MockBrowserModalDialog() override {
    BrowserModalDialogList::GetInstance()->RemoveDialog(this);
  }
  MOCK_METHOD1(ActivateModalDialog, void(Browser*));
  MOCK_METHOD0(IsShowing, bool());
};

class BrowserModalDialogTest : public ::testing::Test {
 public:
  BrowserModalDialogTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserModalDialogTest);
};

TEST_F(BrowserModalDialogTest, ShowDialog) {
  BrowserModalDialogList* dialog_list = BrowserModalDialogList::GetInstance();
  ASSERT_FALSE(dialog_list->IsShowing());
  MockBrowserModalDialog dialog;
  EXPECT_CALL(dialog, IsShowing()).Times(2).WillRepeatedly(Return(true));
  EXPECT_CALL(dialog, ActivateModalDialog(_)).Times(1);
  ASSERT_TRUE(dialog_list->IsShowing());
  dialog_list->ActivateModalDialog(nullptr);
}

TEST_F(BrowserModalDialogTest, RemoveDialog) {
  BrowserModalDialogList* dialog_list = BrowserModalDialogList::GetInstance();
  std::unique_ptr<MockBrowserModalDialog> dialog =
      base::MakeUnique<MockBrowserModalDialog>();
  EXPECT_CALL(*dialog, IsShowing()).Times(1).WillRepeatedly(Return(true));
  ASSERT_TRUE(dialog_list->IsShowing());
  dialog.reset();
  ASSERT_FALSE(dialog_list->IsShowing());
}
