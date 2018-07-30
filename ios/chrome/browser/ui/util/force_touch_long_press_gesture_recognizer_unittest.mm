// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/force_touch_long_press_gesture_recognizer.h"

#import <UIKit/UIGestureRecognizerSubclass.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ForceTouchLongPressGestureRecognizerReceiverForTest : NSObject

@end

@implementation ForceTouchLongPressGestureRecognizerReceiverForTest

- (void)handleGestureRecognizer:(UIGestureRecognizer*)gesture {
}

@end

namespace {

using ForceTouchLongPressGestureRecognizerTest = PlatformTest;

TEST_F(ForceTouchLongPressGestureRecognizerTest, DetectForceTouch) {
  ForceTouchLongPressGestureRecognizerReceiverForTest* testReceiver =
      [[ForceTouchLongPressGestureRecognizerReceiverForTest alloc] init];

  ForceTouchLongPressGestureRecognizer* gestureRecognizer =
      [[ForceTouchLongPressGestureRecognizer alloc]
          initWithTarget:testReceiver
                  action:@selector(handleGestureRecognizer:)];
  gestureRecognizer.forceThreshold = 0.6;

  ASSERT_EQ(UIGestureRecognizerStatePossible, gestureRecognizer.state);

  id event = OCMClassMock([UIEvent class]);

  id touch = OCMClassMock([UITouch class]);
  OCMStub([touch maximumPossibleForce]).andReturn(1);
  OCMStub([touch force]).andReturn(0.5);
  [gestureRecognizer touchesBegan:[NSSet setWithObject:touch] withEvent:event];
  [gestureRecognizer touchesMoved:[NSSet setWithObject:touch] withEvent:event];

  EXPECT_EQ(UIGestureRecognizerStatePossible, gestureRecognizer.state);

  touch = OCMClassMock([UITouch class]);
  OCMStub([touch maximumPossibleForce]).andReturn(1);
  OCMStub([touch force]).andReturn(0.7);
  [gestureRecognizer touchesMoved:[NSSet setWithObject:touch] withEvent:event];

  EXPECT_EQ(UIGestureRecognizerStateBegan, gestureRecognizer.state);

  touch = OCMClassMock([UITouch class]);
  OCMStub([touch maximumPossibleForce]).andReturn(1);
  OCMStub([touch force]).andReturn(0.7);
  [gestureRecognizer touchesEnded:[NSSet setWithObject:touch] withEvent:event];

  EXPECT_EQ(UIGestureRecognizerStateCancelled, gestureRecognizer.state);
}

}  // namespace
