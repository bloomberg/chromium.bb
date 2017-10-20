// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/signin_promo_view_controller.h"

#import "chrome/browser/ui/cocoa/passwords/base_passwords_controller_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"

namespace {

class SignInPromoViewControllerTest : public ManagePasswordsControllerTest {
 public:
  void SetUp() override;
  void TearDown() override;

  void SetUpSignInPromoState();

  SignInPromoViewController* controller() { return controller_.get(); }

 private:
  base::scoped_nsobject<SignInPromoViewController> controller_;
};

void SignInPromoViewControllerTest::SetUp() {
  ManagePasswordsControllerTest::SetUp();
  SetUpSignInPromoState();
}

void SignInPromoViewControllerTest::TearDown() {
  ManagePasswordsControllerTest::TearDown();
}

void SignInPromoViewControllerTest::SetUpSignInPromoState() {
  SetUpSavePendingState();
  GetModelAndCreateIfNull()->OnSaveClicked();

  ASSERT_TRUE(GetModelAndCreateIfNull()->ReplaceToShowPromotionIfNeeded());

  [delegate() setModel:GetModelAndCreateIfNull()];
  controller_.reset([[SignInPromoViewController alloc]
      initWithDelegate:delegate()]);
  [controller_ view];
}


TEST_F(SignInPromoViewControllerTest, ClickSignIn) {
  EXPECT_CALL(*ui_controller(), NavigateToChromeSignIn());
  [controller().signInButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(SignInPromoViewControllerTest, ClickNo) {
  EXPECT_CALL(*ui_controller(), NavigateToChromeSignIn()).Times(0);
  [controller().noButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(SignInPromoViewControllerTest, ClickClose) {
  EXPECT_CALL(*ui_controller(), NavigateToChromeSignIn()).Times(0);
  [controller().closeButton performClick:nil];

  EXPECT_TRUE([delegate() dismissed]);
}

TEST_F(SignInPromoViewControllerTest, CloseBubbleAndHandleClick) {
  // A user may press mouse down, some navigation closes the bubble, mouse up
  // still sends the action.
  EXPECT_CALL(*ui_controller(), NavigateToChromeSignIn()).Times(0);
  [delegate() setModel:nil];
  [controller().signInButton performClick:nil];
  [controller().noButton performClick:nil];
  [controller().closeButton performClick:nil];
}

}  // namespace
