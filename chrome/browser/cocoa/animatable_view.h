// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_ANIMATABLE_VIEW_H_
#define CHROME_BROWSER_COCOA_ANIMATABLE_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/view_resizer.h"

// A view that provides an animatable height property.  Provides methods to
// animate to a new height, set a new height immediately, or cancel any running
// animations.
//
// AnimatableView sends an |animationDidEnd:| message to its delegate when the
// animation ends normally and an |animationDidStop:| message when the animation
// was canceled (even when canceled as a result of a new animation starting).

@interface AnimatableView : NSView {
 @protected
  IBOutlet id delegate_;  // weak, used to send animation ended messages.

 @private
  scoped_nsobject<NSAnimation> currentAnimation_;
  id<ViewResizer> resizeDelegate_;  // weak, usually owns us
}

// Properties for bindings.
@property(assign) id delegate;

// Gets the current height of the view.  If an animation is currently running,
// this will give the current height at the time of the call, not the target
// height at the end of the animation.
- (CGFloat)height;

// Sets the height of the view immediately.  Cancels any running animations.
- (void)setHeight:(CGFloat)newHeight;

// Starts a new animation to the given |newHeight| for the given |duration|.
// Cancels any running animations.
- (void)animateToNewHeight:(CGFloat)newHeight
                  duration:(NSTimeInterval)duration;

// Cancels any running animations, leaving the view at its current
// (mid-animation) height.
- (void)stopAnimation;

// Sets the delegate that gets notified when this view needs to chanage its
// height.
- (void)setResizeDelegate:(id<ViewResizer>)resizeDelegate;

@end

#endif  // CHROME_BROWSER_COCOA_ANIMATABLE_VIEW_H_
