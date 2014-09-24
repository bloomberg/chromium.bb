// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_confirmation_view_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/bubble_combobox.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/passwords/manage_passwords_controller_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/browser/ui/passwords/save_password_refusal_combobox_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

@interface ManagePasswordsBubbleContentViewTestDelegate
    : NSObject<ManagePasswordsBubbleContentViewDelegate> {
  BOOL dismissed_;
}
@property(readonly) BOOL dismissed;
@end

@implementation ManagePasswordsBubbleContentViewTestDelegate

@synthesize dismissed = dismissed_;

- (void)viewShouldDismiss {
  dismissed_ = YES;
}

@end

namespace {

class ManagePasswordsBubbleConfirmationViewControllerTest
    : public ManagePasswordsControllerTest {
 public:
  ManagePasswordsBubbleConfirmationViewControllerTest() : controller_(nil) {}

  virtual void SetUp() OVERRIDE {
    ManagePasswordsControllerTest::SetUp();
    delegate_.reset(
        [[ManagePasswordsBubbleContentViewTestDelegate alloc] init]);
  }

  ManagePasswordsBubbleContentViewTestDelegate* delegate() {
    return delegate_.get();
  }

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
  base::scoped_nsobject<ManagePasswordsBubbleContentViewTestDelegate> delegate_;
};

TEST_F(ManagePasswordsBubbleConfirmationViewControllerTest,
       ShouldDismissWhenOKClicked) {
  [controller().okButton performClick:nil];
  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(ManagePasswordsBubbleConfirmationViewControllerTest,
       ShouldOpenPasswordsAndDismissWhenLinkClicked) {
  [controller().confirmationText clickedOnLink:nil atIndex:0];
  EXPECT_TRUE([delegate() dismissed]);
  EXPECT_TRUE(ui_controller()->navigated_to_settings_page());
}

}  // namespace
