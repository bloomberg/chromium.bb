// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_PAGE_ANIMATION_UTIL_H_
#define IOS_CHROME_BROWSER_UI_UTIL_PAGE_ANIMATION_UTIL_H_

#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/QuartzCore.h>

@class CardView;
@class UIView;

// Utility for handling the animation of a page closing.
namespace page_animation_util {

// Animates |view| to its final position following |delay| seconds, then calls
// the given completion block when finished.
void AnimateOutWithCompletion(UIView* view,
                              NSTimeInterval delay,
                              BOOL clockwise,
                              BOOL isPortrait,
                              void (^completion)(void));

// Moribunt types and constants to be deleted soon.
extern const CGFloat kCardMargin;

enum TabStartPosition { START_RIGHT, START_LEFT };

// Moribund functions to be deleted soon -- it is an error to call them.
void SetTabAnimationStartPositionForView(UIView* view, TabStartPosition start);

void AnimateInCardWithAnimationAndCompletion(UIView* view,
                                             void (^extraAnimation)(void),
                                             void (^completion)(void));

void AnimateInPaperWithAnimationAndCompletion(UIView* view,
                                              CGFloat paperOffset,
                                              CGFloat contentOffset,
                                              CGPoint origin,
                                              BOOL isOffTheRecord,
                                              void (^extraAnimation)(void),
                                              void (^completion)(void));

void AnimateNewBackgroundPageWithCompletion(CardView* currentPageCard,
                                            CGRect displayFrame,
                                            CGRect imageFrame,
                                            BOOL isPortrait,
                                            void (^completion)(void));

void AnimateNewBackgroundTabWithCompletion(CardView* currentPageCard,
                                           CardView* newCard,
                                           CGRect displayFrame,
                                           BOOL isPortrait,
                                           void (^completion)(void));

CGAffineTransform AnimateOutTransform(CGFloat fraction,
                                      BOOL clockwise,
                                      BOOL isPortrait);

CGFloat AnimateOutTransformBreadth();

}  // namespace page_animation_util

#endif  // IOS_CHROME_BROWSER_UI_UTIL_PAGE_ANIMATION_UTIL_H_
