// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_ANIMATION_H_
#define CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_ANIMATION_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"

// Base class for all constrained window animation classes.
@interface ConstrainedWindowAnimationBase : NSAnimation {
 @protected
  base::scoped_nsobject<NSWindow> window_;
}

- (id)initWithWindow:(NSWindow*)window;

@end

// An animation to show a window.
@interface ConstrainedWindowAnimationShow : ConstrainedWindowAnimationBase
@end

// An animation to hide a window.
@interface ConstrainedWindowAnimationHide : ConstrainedWindowAnimationBase
@end

// An animation that pulses the window by growing it then shrinking it back.
@interface ConstrainedWindowAnimationPulse : ConstrainedWindowAnimationBase
@end

#endif  // CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_ANIMATION_H_
