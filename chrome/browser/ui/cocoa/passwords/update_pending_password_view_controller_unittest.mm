// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/pending_password_view_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/bubble_combobox.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/passwords/base_passwords_controller_test.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_list_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/credentials_selection_view.h"
#import "chrome/browser/ui/cocoa/passwords/update_pending_password_view_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

using testing::_;

namespace {

class UpdatePendingPasswordViewControllerTest
    : public ManagePasswordsControllerTest {
 public:
  UpdatePendingPasswordViewControllerTest() {}

  void SetUp() override {
    ManagePasswordsControllerTest::SetUp();
    delegate_.reset([[ContentViewDelegateMock alloc] init]);
  }

  ContentViewDelegateMock* delegate() { return delegate_.get(); }

  UpdatePendingPasswordViewController* controller() {
    if (!controller_) {
      [delegate() setModel:GetModelAndCreateIfNull()];
      controller_.reset([[UpdatePendingPasswordViewController alloc]
          initWithDelegate:delegate()]);
      [controller_ view];
    }
    return controller_.get();
  }

 private:
  base::scoped_nsobject<UpdatePendingPasswordViewController> controller_;
  base::scoped_nsobject<ContentViewDelegateMock> delegate_;
};

TEST_F(UpdatePendingPasswordViewControllerTest,
       ShouldUpdatePasswordAndDismissWhenUpdateClicked) {
  SetUpUpdatePendingState(false);
  EXPECT_CALL(*ui_controller(), UpdatePassword(_));
  EXPECT_CALL(*ui_controller(), OnNopeUpdateClicked()).Times(0);
  [controller().updateButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(UpdatePendingPasswordViewControllerTest,
       ShouldNopeAndDismissWhenNopeClicked) {
  SetUpUpdatePendingState(false);
  EXPECT_CALL(*ui_controller(), UpdatePassword(_)).Times(0);
  EXPECT_CALL(*ui_controller(), OnNopeUpdateClicked());
  [controller().noButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(UpdatePendingPasswordViewControllerTest, ShouldDismissWhenCrossClicked) {
  SetUpUpdatePendingState(false);
  EXPECT_CALL(*ui_controller(), UpdatePassword(_)).Times(0);
  EXPECT_CALL(*ui_controller(), OnNopeUpdateClicked()).Times(0);
  [controller().closeButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(UpdatePendingPasswordViewControllerTest,
       ShouldShowUsernameLabelWhenOneCredentials) {
  SetUpUpdatePendingState(false);
  EXPECT_NE([CredentialsSelectionView class],
            [[controller() createPasswordView] class]);
}

TEST_F(UpdatePendingPasswordViewControllerTest,
       ShouldShowUsernamesComboboxWhenMultipleCredentials) {
  SetUpUpdatePendingState(true);
  EXPECT_EQ([CredentialsSelectionView class],
            [[controller() createPasswordView] class]);
}

TEST_F(UpdatePendingPasswordViewControllerTest, CloseBubbleAndHandleClick) {
  // A user may press mouse down, some navigation closes the bubble, mouse up
  // still sends the action.
  SetUpUpdatePendingState(false);
  EXPECT_CALL(*ui_controller(), UpdatePassword(_)).Times(0);
  EXPECT_CALL(*ui_controller(), OnNopeUpdateClicked()).Times(0);
  [controller() bubbleWillDisappear];
  [delegate() setModel:nil];
  [controller().updateButton performClick:nil];
  [controller().noButton performClick:nil];
}

}  // namespace
