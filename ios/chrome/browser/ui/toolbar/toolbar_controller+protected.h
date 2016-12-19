// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_PROTECTED_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_PROTECTED_H_

#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"

@interface ToolbarController (Protected)

// Animation key used for toolbar transition animations.
extern NSString* const kToolbarTransitionAnimationKey;

// An array of CALayers that are currently animating under
// kToolbarTransitionAnimationKey.
@property(nonatomic, readonly) NSMutableArray* transitionLayers;

// Adds transition animations to every UIButton in |containerView| with a
// nonzero opacity.
- (void)animateTransitionForButtonsInView:(UIView*)containerView
                     containerBeginBounds:(CGRect)containerBeginBounds
                       containerEndBounds:(CGRect)containerEndBounds
                          transitionStyle:(ToolbarTransitionStyle)style;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_PROTECTED_H_
