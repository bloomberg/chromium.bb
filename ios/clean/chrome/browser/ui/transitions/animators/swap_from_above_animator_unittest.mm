// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/transitions/animators/swap_from_above_animator.h"

#import "base/test/ios/wait_util.h"
#import "ios/clean/chrome/browser/ui/transitions/animators/test_transition_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using SwapFromAboveAnimatorTest = PlatformTest;

TEST_F(SwapFromAboveAnimatorTest, AnimationCallsCompleteTransitionOnContext) {
  SwapFromAboveAnimator* animator = [[SwapFromAboveAnimator alloc] init];
  TestTransitionContext* context = [[TestTransitionContext alloc] init];

  [animator animateTransition:context];

  base::test::ios::WaitUntilCondition(^bool {
    return context.completeTransitionCount == 1;
  });
}
