// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_ANIMATORS_TEST_TRANSITION_CONTEXT_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_ANIMATORS_TEST_TRANSITION_CONTEXT_H_

#import <UIKit/UIKit.h>

// A concrete transitioning context for tests.
@interface TestTransitionContext
    : NSObject<UIViewControllerContextTransitioning>

// Counts the number of times |completeTransition:| was called.
@property(nonatomic) NSUInteger completeTransitionCount;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_ANIMATORS_TEST_TRANSITION_CONTEXT_H_
