// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/confirmation_password_saved_view_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/bubble_combobox.h"
#include "chrome/browser/ui/cocoa/passwords/base_passwords_controller_test.h"
#include "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace {

class ConfirmationPasswordSavedViewControllerTest
    : public ManagePasswordsControllerTest {
 public:
  void SetUp() override {
    ManagePasswordsControllerTest::SetUp();
    SetUpConfirmationState();
    controller_.reset([[ConfirmationPasswordSavedViewController alloc]
        initWithDelegate:delegate()]);
    [controller_ view];
  }

  ConfirmationPasswordSavedViewController* controller() {
    return controller_.get();
  }

 private:
  base::scoped_nsobject<ConfirmationPasswordSavedViewController> controller_;
};

TEST_F(ConfirmationPasswordSavedViewControllerTest,
       ShouldDismissWhenOKClicked) {
  [controller().okButton performClick:nil];
  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(ConfirmationPasswordSavedViewControllerTest,
       ShouldOpenPasswordsAndDismissWhenLinkClicked) {
  EXPECT_CALL(*ui_controller(), NavigateToPasswordManagerAccountDashboard());
  [controller().confirmationText clickedOnLink:@"about:blank" atIndex:0];
  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(ConfirmationPasswordSavedViewControllerTest, CloseBubbleAndHandleClick) {
  // A user may press mouse down, some navigation closes the bubble, mouse up
  // still sends the action.
  EXPECT_CALL(*ui_controller(), NavigateToPasswordManagerAccountDashboard())
      .Times(0);
  [delegate() setModel:nil];
  [controller().confirmationText clickedOnLink:@"about:blank" atIndex:0];
  [controller().okButton performClick:nil];
}

}  // namespace
