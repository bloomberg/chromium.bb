// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/non_modal/non_modal_alert_touch_forwarder.h"

#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The length of the test view's size.
const CGFloat kViewSize = 400.0;
// The inset to use for the mask.
const CGFloat kMaskInset = 50.0;
}

// Test fixture for the NonModalAlertTouchForwarder.
class NonModalAlertTouchForwarderTest : public PlatformTest {
 public:
  NonModalAlertTouchForwarderTest()
      : container_view_([[UIView alloc] initWithFrame:CGRectZero]),
        background_view_([[UIView alloc] initWithFrame:CGRectZero]),
        foreground_view_([[UIView alloc] initWithFrame:CGRectZero]),
        mask_([CALayer layer]),
        touch_forwarder_([[NonModalAlertTouchForwarder alloc] init]) {
    touch_forwarder_.mask = mask_;
    touch_forwarder_.forwardingTarget = background_view_;
    container_view_.frame = CGRectMake(0, 0, kViewSize, kViewSize);
    background_view_.frame = container_view_.bounds;
    [container_view_ addSubview:background_view_];
    foreground_view_.frame = background_view_.bounds;
    [container_view_ addSubview:foreground_view_];
    mask_.backgroundColor = [UIColor blackColor].CGColor;
    mask_.frame = UIEdgeInsetsInsetRect(
        foreground_view_.bounds,
        UIEdgeInsetsMake(kMaskInset, kMaskInset, kMaskInset, kMaskInset));
    foreground_view_.layer.mask = mask_;
    [foreground_view_ addSubview:touch_forwarder_];
  }

  // Returns the hit view in the test hierarchy at |location|.
  UIView* GetHitView(CGPoint location) {
    return [container_view_ hitTest:location withEvent:nil];
  }

 protected:
  UIView* container_view_ = nil;
  UIView* background_view_ = nil;
  UIView* foreground_view_ = nil;
  CALayer* mask_ = nil;
  NonModalAlertTouchForwarder* touch_forwarder_ = nil;
};

// Tests that |-hitTest:withEvent:| for touches within the mask are routed to
// the foreground view.
TEST_F(NonModalAlertTouchForwarderTest, TouchWithinMask) {
  const CGPoint kTouchPoint = CGPointMake(kViewSize / 2.0, kViewSize / 2.0);
  EXPECT_EQ(GetHitView(kTouchPoint), foreground_view_);
}

// Tests that |-hitTest:withEvent:| for touches outside of the mask are routed
// to the background view.
TEST_F(NonModalAlertTouchForwarderTest, TouchOutsideMask) {
  const CGPoint kTouchPoint = CGPointMake(kMaskInset / 2.0, kMaskInset / 2.0);
  EXPECT_EQ(GetHitView(kTouchPoint), background_view_);
}
