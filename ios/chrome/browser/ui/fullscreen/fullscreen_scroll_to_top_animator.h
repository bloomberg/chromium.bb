// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_SCROLL_TO_TOP_ANIMATOR_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_SCROLL_TO_TOP_ANIMATOR_H_

#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"

// When the user taps on the status bar, scroll views are expected to scroll to
// top.  This animator helps to coordinate that animation with a toolbar reveal
// animation.
@interface FullscreenScrollToTopAnimator : FullscreenAnimator

- (instancetype)initWithStartProgress:(CGFloat)startProgress
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStartProgress:(CGFloat)startProgress
                             duration:(NSTimeInterval)duration NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_SCROLL_TO_TOP_ANIMATOR_H_
