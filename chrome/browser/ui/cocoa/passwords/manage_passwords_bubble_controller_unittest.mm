// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_controller.h"

#import <Cocoa/Cocoa.h>

#include "base/compiler_specific.h"
#include "base/mac/foundation_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/passwords/manage_passwords_controller_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace {

class ManagePasswordsBubbleControllerTest
    : public ManagePasswordsControllerTest {
 public:
  ManagePasswordsBubbleControllerTest() : controller_(nil) {}

  virtual void SetUp() OVERRIDE {
    ManagePasswordsControllerTest::SetUp();
    model()->set_state(password_manager::ui::PENDING_PASSWORD_STATE);
  }

  ManagePasswordsBubbleController* controller() {
    if (!controller_) {
      controller_ = [[ManagePasswordsBubbleController alloc]
          initWithParentWindow:browser()->window()->GetNativeWindow()
                         model:model()];
    }
    return controller_;
  }

 private:
  ManagePasswordsBubbleController* controller_;  // weak; owns itself.
};

TEST_F(ManagePasswordsBubbleControllerTest, PendingStateShouldHavePendingView) {
  // We start in the pending state.
  EXPECT_EQ([ManagePasswordsBubblePendingViewController class],
            [[controller() currentController] class]);
}

TEST_F(ManagePasswordsBubbleControllerTest, DismissingShouldCloseWindow) {
  [controller() showWindow:nil];

  // Turn off animations so that closing happens immediately.
  base::scoped_nsobject<InfoBubbleWindow> window(
      [base::mac::ObjCCast<InfoBubbleWindow>([controller() window]) retain]);
  [window setAllowedAnimations:info_bubble::kAnimateNone];

  EXPECT_TRUE([window isVisible]);
  [controller() viewShouldDismiss];
  EXPECT_FALSE([window isVisible]);
}

}  // namespace
