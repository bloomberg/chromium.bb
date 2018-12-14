// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/non_modal/non_modal_alert_presentation_updater.h"

#import "base/mac/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/dialogs/non_modal/non_modal_alert_touch_forwarder.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForUIElementTimeout;

// Test fixture for the NonModalAlertPresentationController.
class NonModalAlertPresentationControllerTest : public PlatformTest {
 protected:
  NonModalAlertPresentationControllerTest()
      : window_([[UIWindow alloc] init]),
        view_controller_([[UIViewController alloc] init]),
        alert_controller_([UIAlertController
            alertControllerWithTitle:@"Test Alert"
                             message:@"Message"
                      preferredStyle:UIAlertControllerStyleAlert]),
        presentation_updater_([[NonModalAlertPresentationUpdater alloc]
            initWithAlertController:alert_controller_]) {
    window_.frame = CGRectMake(0, 0, 400, 400);
    window_.rootViewController = view_controller_;
    original_key_window_ = [UIApplication sharedApplication].keyWindow;
    [window_ makeKeyAndVisible];
    [alert_controller_
        addAction:[UIAlertAction
                      actionWithTitle:@"OK"
                                style:UIAlertActionStyleDefault
                              handler:^(UIAlertAction* _Nonnull action){
                              }]];
  }

  ~NonModalAlertPresentationControllerTest() override {
    [window_ resignKeyWindow];
    [original_key_window_ makeKeyAndVisible];
  }

  // Presents the alert and waits for the presentation to be completed.
  void PresentAlert() {
    __block bool presented = false;
    [view_controller_ presentViewController:alert_controller_
                                   animated:YES
                                 completion:^{
                                   presented = true;
                                 }];
    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForUIElementTimeout, ^{
          return presented;
        }));
  }

  UIWindow* original_key_window_ = nil;
  UIWindow* window_ = nil;
  UIViewController* view_controller_ = nil;
  UIAlertController* alert_controller_ = nil;
  NonModalAlertPresentationUpdater* presentation_updater_ = nil;
};

// Verifies that the presentation updater sets up the mask and touch forwarder
// correctly.
TEST_F(NonModalAlertPresentationControllerTest, SetUp) {
  const UIEdgeInsets kInsets = UIEdgeInsetsMake(50, 50, 50, 50);
  PresentAlert();
  [presentation_updater_ setUpNonModalPresentationWithViewportInsets:kInsets];
  UIView* presentation_container_view =
      alert_controller_.presentationController.containerView;
  CALayer* presentation_container_layer = presentation_container_view.layer;
  // Check that the presentation container is masked.
  CALayer* mask = presentation_container_layer.mask;
  EXPECT_TRUE(mask);
  EXPECT_TRUE(CGRectEqualToRect(
      mask.frame,
      UIEdgeInsetsInsetRect(presentation_container_layer.bounds, kInsets)));
  // Check that the touch forwarder has been inserted.
  NonModalAlertTouchForwarder* touch_forwarder = nil;
  for (UIView* subview in presentation_container_view.subviews) {
    touch_forwarder = base::mac::ObjCCast<NonModalAlertTouchForwarder>(subview);
    if (touch_forwarder)
      break;
  }
  EXPECT_TRUE(touch_forwarder);
  EXPECT_EQ(touch_forwarder.mask, mask);
  EXPECT_EQ(touch_forwarder.forwardingTarget, view_controller_.view);
}

// Verifies that the presentation updater updates the mask for fullscreen inset
// changes.
TEST_F(NonModalAlertPresentationControllerTest, UpdateMask) {
  const UIEdgeInsets kInsets = UIEdgeInsetsMake(50, 50, 50, 50);
  PresentAlert();
  [presentation_updater_ setUpNonModalPresentationWithViewportInsets:kInsets];
  // Check that the presentation container is masked.
  UIView* presentation_container_view =
      alert_controller_.presentationController.containerView;
  CALayer* presentation_container_layer = presentation_container_view.layer;
  CALayer* mask = presentation_container_layer.mask;
  ASSERT_TRUE(mask);
  ASSERT_TRUE(CGRectEqualToRect(
      mask.frame,
      UIEdgeInsetsInsetRect(presentation_container_layer.bounds, kInsets)));
  // Check that the mask frame is updated for new insets.
  const UIEdgeInsets kNewInsets = UIEdgeInsetsMake(15, 50, 15, 50);
  [presentation_updater_ updateForFullscreenMinViewportInsets:kNewInsets
                                            maxViewportInsets:kNewInsets];
  EXPECT_TRUE(CGRectEqualToRect(
      mask.frame,
      UIEdgeInsetsInsetRect(presentation_container_layer.bounds, kNewInsets)));
}
