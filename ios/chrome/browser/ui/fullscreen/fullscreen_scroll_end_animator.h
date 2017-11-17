// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_SCROLL_END_ANIMATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_SCROLL_END_ANIMATOR_H_

#import <UIKit/UIKit.h>

#if defined(__IPHONE_10_0) && (__IPHONE_OS_VERSION_MIN_ALLOWED >= __IPHONE_10_0)
// When a scroll event ends, the toolbar should be either completely hidden or
// completely visible.  If a scroll ends and the toolbar is partly visible, this
// animator will be provided to UI elements to animate its state to a hidden or
// visible state.
@interface FullscreenScrollEndAnimator : UIViewPropertyAnimator

// The progress value at the start of the animation.
@property(nonatomic, readonly) CGFloat startProgress;
// The final calculated fullscreen value.
@property(nonatomic, readonly) CGFloat finalProgress;
// The current progress value.  This is the fullscreen progress value
// interpolated between |startProgress| and |finalProgress| using the timing
// curve and the fraction complete of the animation.
@property(nonatomic, readonly) CGFloat currentProgress;

// Designated initializer.
- (instancetype)initWithStartProgress:(CGFloat)startProgress
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithDuration:(NSTimeInterval)duration
                timingParameters:(id<UITimingCurveProvider>)parameters
    NS_UNAVAILABLE;

@end

#else

// Dummy object.
// TODO(crbug.com/768876): Remove this class and the #if guards once iOS9
// support is dropped.
@interface FullscreenScrollEndAnimator : NSObject
@end

#endif  // __IPHONE_10_0

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_SCROLL_END_ANIMATOR_H_
