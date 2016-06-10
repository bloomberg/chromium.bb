// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/passwords/passwords_bubble_controller.h"

#import <Cocoa/Cocoa.h>

#include "base/compiler_specific.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/passwords/base_passwords_controller_test.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/save_pending_password_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/signin_promo_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/update_pending_password_view_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace {

using password_bubble_experiment::kChromeSignInPasswordPromoExperimentName;
using password_bubble_experiment::kChromeSignInPasswordPromoThresholdParam;

class ManagePasswordsBubbleControllerTest
    : public ManagePasswordsControllerTest {
 public:
  ManagePasswordsBubbleControllerTest() : controller_(nil) {}

  ManagePasswordsBubbleController* controller() {
    if (!controller_) {
      controller_ = [[ManagePasswordsBubbleController alloc]
          initWithParentWindow:browser()->window()->GetNativeWindow()
                         model:GetModelAndCreateIfNull()];
    }
    return controller_;
  }

  ManagePasswordsBubbleModel::DisplayReason GetDisplayReason() const override {
    return ManagePasswordsBubbleModel::USER_ACTION;
  }

 private:
  ManagePasswordsBubbleController* controller_;  // weak; owns itself.
};

TEST_F(ManagePasswordsBubbleControllerTest, PendingStateShouldHavePendingView) {
  SetUpSavePendingState(false);
  EXPECT_EQ([SavePendingPasswordViewController class],
            [[controller() currentController] class]);
}

TEST_F(ManagePasswordsBubbleControllerTest, DismissingShouldCloseWindow) {
  SetUpSavePendingState(false);
  [controller() showWindow:nil];

  // Turn off animations so that closing happens immediately.
  base::scoped_nsobject<InfoBubbleWindow> window(
      [base::mac::ObjCCast<InfoBubbleWindow>([controller() window]) retain]);
  [window setAllowedAnimations:info_bubble::kAnimateNone];

  EXPECT_TRUE([window isVisible]);
  [controller() viewShouldDismiss];
  EXPECT_FALSE([window isVisible]);
}

TEST_F(ManagePasswordsBubbleControllerTest, ManageStateShouldHaveManageView) {
  SetUpManageState(VectorConstFormPtr());
  EXPECT_EQ([ManagePasswordsViewController class],
            [[controller() currentController] class]);
}

TEST_F(ManagePasswordsBubbleControllerTest,
       PendingStateShouldHaveUpdatePendingView) {
  SetUpUpdatePendingState(false);
  EXPECT_EQ([UpdatePendingPasswordViewController class],
            [[controller() currentController] class]);
}

TEST_F(ManagePasswordsBubbleControllerTest, ClearModelOnClose) {
  SetUpUpdatePendingState(false);
  EXPECT_TRUE(controller().model);
  [controller() close];
  EXPECT_FALSE(controller().model);
}

TEST_F(ManagePasswordsBubbleControllerTest, TransitionToSignInPromo) {
  const char kFakeGroup[] = "FakeGroup";
  base::FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      kChromeSignInPasswordPromoExperimentName, kFakeGroup));
  variations::AssociateVariationParams(
      kChromeSignInPasswordPromoExperimentName, kFakeGroup,
      {{kChromeSignInPasswordPromoThresholdParam, "3"}});

  SetUpSavePendingState(false);
  [controller() showWindow:nil];
  SavePendingPasswordViewController* saveController =
      base::mac::ObjCCastStrict<SavePendingPasswordViewController>(
          [controller() currentController]);
  EXPECT_TRUE(saveController.saveButton);
  [saveController.saveButton performClick:nil];
  EXPECT_EQ([SignInPromoViewController class],
            [[controller() currentController] class]);
  EXPECT_TRUE([[controller() window] isVisible]);

  variations::testing::ClearAllVariationParams();
}

}  // namespace
