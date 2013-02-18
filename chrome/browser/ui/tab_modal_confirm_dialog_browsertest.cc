// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_modal_confirm_dialog_browsertest.h"

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

MockTabModalConfirmDialogDelegate::MockTabModalConfirmDialogDelegate(
    content::WebContents* web_contents,
    Delegate* delegate)
    : TabModalConfirmDialogDelegate(web_contents),
      delegate_(delegate) {
}

MockTabModalConfirmDialogDelegate::~MockTabModalConfirmDialogDelegate() {
}

string16 MockTabModalConfirmDialogDelegate::GetTitle() {
  return string16();
}

string16 MockTabModalConfirmDialogDelegate::GetMessage() {
  return string16();
}

void MockTabModalConfirmDialogDelegate::OnAccepted() {
  if (delegate_)
    delegate_->OnAccepted();
}

void MockTabModalConfirmDialogDelegate::OnCanceled() {
  if (delegate_)
    delegate_->OnCanceled();
}

TabModalConfirmDialogTest::TabModalConfirmDialogTest()
    : delegate_(NULL),
      dialog_(NULL),
      accepted_count_(0),
      canceled_count_(0) {
}

void TabModalConfirmDialogTest::SetUpOnMainThread() {
  delegate_ = new MockTabModalConfirmDialogDelegate(
      browser()->tab_strip_model()->GetActiveWebContents(), this);
  dialog_ = TabModalConfirmDialog::Create(
      delegate_, browser()->tab_strip_model()->GetActiveWebContents());
  content::RunAllPendingInMessageLoop();
}

void TabModalConfirmDialogTest::CleanUpOnMainThread() {
  content::RunAllPendingInMessageLoop();
}

void TabModalConfirmDialogTest::OnAccepted() {
  ++accepted_count_;
}

void TabModalConfirmDialogTest::OnCanceled() {
  ++canceled_count_;
}

IN_PROC_BROWSER_TEST_F(TabModalConfirmDialogTest, Accept) {
  dialog_->AcceptTabModalDialog();
  EXPECT_EQ(1, accepted_count_);
  EXPECT_EQ(0, canceled_count_);
}

IN_PROC_BROWSER_TEST_F(TabModalConfirmDialogTest, Cancel) {
  dialog_->CancelTabModalDialog();
  EXPECT_EQ(0, accepted_count_);
  EXPECT_EQ(1, canceled_count_);
}

IN_PROC_BROWSER_TEST_F(TabModalConfirmDialogTest, CancelSelf) {
  delegate_->Cancel();
  EXPECT_EQ(0, accepted_count_);
  EXPECT_EQ(1, canceled_count_);
}

IN_PROC_BROWSER_TEST_F(TabModalConfirmDialogTest, Quit) {
  MessageLoopForUI::current()->PostTask(FROM_HERE,
                                        base::Bind(&chrome::AttemptExit));
  content::RunMessageLoop();
  EXPECT_EQ(0, accepted_count_);
  EXPECT_EQ(1, canceled_count_);
}
