// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_TOOLBAR_REVEAL_ANIMATOR_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_TOOLBAR_REVEAL_ANIMATOR_H_

#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"

// A FullscreenAnimator used to show the toolbar.
@interface ToolbarRevealAnimator : FullscreenAnimator

- (instancetype)initWithStartProgress:(CGFloat)startProgress
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStartProgress:(CGFloat)startProgress
                             duration:(NSTimeInterval)duration NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_TOOLBAR_REVEAL_ANIMATOR_H_
