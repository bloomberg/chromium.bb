// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/confirmation_password_saved_view_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/bubble_combobox.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/passwords/base_passwords_controller_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace {

class ManagePasswordsBubbleConfirmationViewControllerTest
    : public ManagePasswordsControllerTest {
 public:
  ManagePasswordsBubbleConfirmationViewControllerTest() : controller_(nil) {}

  void SetUp() override {
    ManagePasswordsControllerTest::SetUp();
    delegate_.reset([[ContentViewDelegateMock alloc] init]);
  }

  ContentViewDelegateMock* delegate() { return delegate_.get(); }

  ManagePasswordsBubbleConfirmationViewController* controller() {
    if (!controller_) {
      controller_.reset([[ManagePasswordsBubbleConfirmationViewController alloc]
          initWithModel:model()
               delegate:delegate()]);
      [controller_ loadView];
    }
    return controller_.get();
  }

 private:
  base::scoped_nsobject<ManagePasswordsBubbleConfirmationViewController>
      controller_;
  base::scoped_nsobject<ContentViewDelegateMock> delegate_;
};

TEST_F(ManagePasswordsBubbleConfirmationViewControllerTest,
       ShouldDismissWhenOKClicked) {
  [controller().okButton performClick:nil];
  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(ManagePasswordsBubbleConfirmationViewControllerTest,
       ShouldOpenPasswordsAndDismissWhenLinkClicked) {
  [controller().confirmationText clickedOnLink:@"about:blank" atIndex:0];
  EXPECT_TRUE([delegate() dismissed]);
  EXPECT_TRUE(ui_controller()->navigated_to_settings_page());
}

}  // namespace
