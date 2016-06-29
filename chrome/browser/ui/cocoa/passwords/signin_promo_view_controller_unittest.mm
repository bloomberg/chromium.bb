// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/signin_promo_view_controller.h"

#include "base/metrics/field_trial.h"
#import "chrome/browser/ui/cocoa/passwords/base_passwords_controller_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/variations/variations_associated_data.h"

namespace {

using password_bubble_experiment::kChromeSignInPasswordPromoExperimentName;
using password_bubble_experiment::kChromeSignInPasswordPromoThresholdParam;

class SignInPromoViewControllerTest : public ManagePasswordsControllerTest {
 public:
  SignInPromoViewControllerTest() : field_trial_list_(nullptr) {}

  void SetUp() override;
  void TearDown() override;

  void SetUpSignInPromoState();

  SignInPromoViewController* controller() { return controller_.get(); }

 private:
  base::FieldTrialList field_trial_list_;
  base::scoped_nsobject<SignInPromoViewController> controller_;
};

void SignInPromoViewControllerTest::SetUp() {
  ManagePasswordsControllerTest::SetUp();
  SetUpSignInPromoState();
}

void SignInPromoViewControllerTest::TearDown() {
  ManagePasswordsControllerTest::TearDown();
  variations::testing::ClearAllVariationParams();
}

void SignInPromoViewControllerTest::SetUpSignInPromoState() {
  const char kFakeGroup[] = "FakeGroup";
  SetUpSavePendingState(false);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      kChromeSignInPasswordPromoExperimentName, kFakeGroup));
  variations::AssociateVariationParams(
      kChromeSignInPasswordPromoExperimentName, kFakeGroup,
      {{kChromeSignInPasswordPromoThresholdParam, "3"}});
  GetModelAndCreateIfNull()->OnSaveClicked();

  ASSERT_TRUE(GetModelAndCreateIfNull()->ReplaceToShowSignInPromoIfNeeded());

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
