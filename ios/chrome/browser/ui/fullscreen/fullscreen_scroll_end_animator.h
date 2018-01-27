// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_SCROLL_END_ANIMATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_SCROLL_END_ANIMATOR_H_

#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"

// When a scroll event ends, the toolbar should be either completely hidden or
// completely visible.  If a scroll ends and the toolbar is partly visible, this
// animator will be provided to UI elements to animate its state to a hidden or
// visible state.
@interface FullscreenScrollEndAnimator : FullscreenAnimator

- (instancetype)initWithStartProgress:(CGFloat)startProgress
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStartProgress:(CGFloat)startProgress
                             duration:(NSTimeInterval)duration NS_UNAVAILABLE;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_SCROLL_END_ANIMATOR_H_
