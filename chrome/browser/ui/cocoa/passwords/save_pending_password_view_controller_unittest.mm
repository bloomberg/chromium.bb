// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/pending_password_view_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/bubble_combobox.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/passwords/base_passwords_controller_test.h"
#import "chrome/browser/ui/cocoa/passwords/save_pending_password_view_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace {

class SavePendingPasswordViewControllerTest
    : public ManagePasswordsControllerTest {
 public:
  SavePendingPasswordViewControllerTest() {}

  void SetUp() override {
    ManagePasswordsControllerTest::SetUp();
    delegate_.reset([[ContentViewDelegateMock alloc] init]);
  }

  ContentViewDelegateMock* delegate() { return delegate_.get(); }

  SavePendingPasswordViewController* controller() {
    if (!controller_) {
      [delegate() setModel:GetModelAndCreateIfNull()];
      controller_.reset([[SavePendingPasswordViewController alloc]
          initWithDelegate:delegate()]);
      [controller_ view];
    }
    return controller_.get();
  }

 private:
  base::scoped_nsobject<SavePendingPasswordViewController> controller_;
  base::scoped_nsobject<ContentViewDelegateMock> delegate_;
};

TEST_F(SavePendingPasswordViewControllerTest,
       ShouldSavePasswordAndDismissWhenSaveClicked) {
  SetUpSavePendingState(false);
  EXPECT_CALL(*ui_controller(), SavePassword());
  EXPECT_CALL(*ui_controller(), NeverSavePassword()).Times(0);
  [controller().saveButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(SavePendingPasswordViewControllerTest,
       ShouldNeverAndDismissWhenNeverClicked) {
  SetUpSavePendingState(false);
  EXPECT_CALL(*ui_controller(), SavePassword()).Times(0);
  EXPECT_CALL(*ui_controller(), NeverSavePassword());
  [controller().neverButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(SavePendingPasswordViewControllerTest, ShouldDismissWhenCrossClicked) {
  SetUpSavePendingState(false);
  EXPECT_CALL(*ui_controller(), SavePassword()).Times(0);
  EXPECT_CALL(*ui_controller(), NeverSavePassword()).Times(0);
  [controller().closeButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(SavePendingPasswordViewControllerTest,
       ShouldShowPasswordRowWhenUsernameNonEmpty) {
  SetUpSavePendingState(false);
  EXPECT_TRUE([controller() createPasswordView]);
}

TEST_F(SavePendingPasswordViewControllerTest,
       ShouldNotShowPasswordRowWhenUsernameEmpty) {
  SetUpSavePendingState(true);
  EXPECT_FALSE([controller() createPasswordView]);
}

TEST_F(SavePendingPasswordViewControllerTest, CloseBubbleAndHandleClick) {
  // A user may press mouse down, some navigation closes the bubble, mouse up
  // still sends the action.
  SetUpSavePendingState(false);
  EXPECT_CALL(*ui_controller(), SavePassword()).Times(0);
  EXPECT_CALL(*ui_controller(), NeverSavePassword()).Times(0);
  [controller() bubbleWillDisappear];
  [delegate() setModel:nil];
  [controller().neverButton performClick:nil];
  [controller().saveButton performClick:nil];
}

}  // namespace
