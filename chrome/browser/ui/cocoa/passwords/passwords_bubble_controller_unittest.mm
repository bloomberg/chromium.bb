// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/passwords/passwords_bubble_controller.h"

#import <Cocoa/Cocoa.h>

#include "base/compiler_specific.h"
#include "base/mac/foundation_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/passwords/base_passwords_controller_test.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/save_pending_password_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/signin_promo_view_controller.h"
#import "chrome/browser/ui/cocoa/passwords/update_pending_password_view_controller.h"
#include "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace {

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
  SetUpSavePendingState();
  EXPECT_EQ([SavePendingPasswordViewController class],
            [[controller() currentController] class]);
}

TEST_F(ManagePasswordsBubbleControllerTest, DismissingShouldCloseWindow) {
  SetUpSavePendingState();
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
  SetUpSavePendingState();
  [controller() showWindow:nil];
  SavePendingPasswordViewController* saveController =
      base::mac::ObjCCastStrict<SavePendingPasswordViewController>(
          [controller() currentController]);
  EXPECT_TRUE(saveController.saveButton);
  [saveController.saveButton performClick:nil];
  EXPECT_EQ([SignInPromoViewController class],
            [[controller() currentController] class]);
  EXPECT_TRUE([[controller() window] isVisible]);
}

// Test that crash doesn't occur after editing the password.
// https://crbug.com/774033
TEST_F(ManagePasswordsBubbleControllerTest,
       TransitionToSignInPromoAfterEditingPassword) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kEnablePasswordSelection);
  SetUpSavePendingState();
  GetModelAndCreateIfNull()->set_hide_eye_icon(false);
  [controller() showWindow:nil];
  SavePendingPasswordViewController* saveController =
      base::mac::ObjCCastStrict<SavePendingPasswordViewController>(
          [controller() currentController]);
  // Edit the password.
  ASSERT_TRUE(saveController.eyeButton);
  [saveController.eyeButton performClick:nil];
  NSPopUpButton* passwordSelection = saveController.passwordSelectionField;
  [passwordSelection selectItemAtIndex:1];

  // Save and move to the promo state.
  [saveController.saveButton performClick:nil];
  EXPECT_EQ([SignInPromoViewController class],
            [[controller() currentController] class]);
  EXPECT_TRUE([[controller() window] isVisible]);
}

}  // namespace
