// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_modal_confirm_dialog_browsertest.h"

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockTabModalConfirmDialogDelegate : public TabModalConfirmDialogDelegate {
 public:
  explicit MockTabModalConfirmDialogDelegate(content::WebContents* web_contents)
      : TabModalConfirmDialogDelegate(web_contents) {}

  virtual string16 GetTitle() OVERRIDE {
    return string16();
  }
  virtual string16 GetMessage() OVERRIDE {
    return string16();
  }

  MOCK_METHOD0(OnAccepted, void());
  MOCK_METHOD0(OnCanceled, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTabModalConfirmDialogDelegate);
};

TabModalConfirmDialogTest::TabModalConfirmDialogTest()
    : delegate_(NULL),
      dialog_(NULL) {}

void TabModalConfirmDialogTest::SetUpOnMainThread() {
  delegate_ = new MockTabModalConfirmDialogDelegate(
      browser()->GetSelectedWebContents());
  dialog_ = CreateTestDialog(delegate_,
                             browser()->GetSelectedTabContentsWrapper());
  ui_test_utils::RunAllPendingInMessageLoop();
}

void TabModalConfirmDialogTest::CleanUpOnMainThread() {
  ui_test_utils::RunAllPendingInMessageLoop();
  ::testing::Mock::VerifyAndClearExpectations(delegate_);
}

// On Mac OS, these methods need to be compiled as Objective-C++, so they're in
// a separate file.
#if !defined(OS_MACOSX)
TabModalConfirmDialog* TabModalConfirmDialogTest::CreateTestDialog(
    TabModalConfirmDialogDelegate* delegate, TabContentsWrapper* wrapper) {
  return new TabModalConfirmDialog(delegate, wrapper);
}

void TabModalConfirmDialogTest::CloseDialog(bool accept) {
#if defined(TOOLKIT_GTK)
  if (accept)
    dialog_->OnAccept(NULL);
  else
    dialog_->OnCancel(NULL);
#else
  if (accept)
    dialog_->GetDialogClientView()->AcceptWindow();
  else
    dialog_->GetDialogClientView()->CancelWindow();
#endif
}
#endif  // !defined(OS_MACOSX)

IN_PROC_BROWSER_TEST_F(TabModalConfirmDialogTest, Accept) {
  EXPECT_CALL(*delegate_, OnAccepted());
  CloseDialog(true);
}

IN_PROC_BROWSER_TEST_F(TabModalConfirmDialogTest, Cancel) {
  EXPECT_CALL(*delegate_, OnCanceled());
  CloseDialog(false);
}

IN_PROC_BROWSER_TEST_F(TabModalConfirmDialogTest, CancelSelf) {
  EXPECT_CALL(*delegate_, OnCanceled());
  delegate_->Cancel();
}

IN_PROC_BROWSER_TEST_F(TabModalConfirmDialogTest, Quit) {
  EXPECT_CALL(*delegate_, OnCanceled());
  MessageLoopForUI::current()->PostTask(FROM_HERE,
                                        base::Bind(&browser::AttemptExit));
  ui_test_utils::RunMessageLoop();
}
