// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/info_bubble_window.h"

#include "base/logging.h"
#include "base/scoped_nsobject.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"

namespace {
const CGFloat kOrderInSlideOffset = 10;
const NSTimeInterval kOrderInAnimationDuration = 0.3;
const NSTimeInterval kOrderOutAnimationDuration = 0.15;
}

@interface InfoBubbleWindow (Private)
- (void)finishCloseAfterAnimation;
@end

// A delegate object for watching the alphaValue animation on InfoBubbleWindows.
// An InfoBubbleWindow instance cannot be the delegate for its own animation
// because CAAnimations retain their delegates, and since the InfoBubbleWindow
// retains its animations a retain loop would be formed.
@interface InfoBubbleWindowCloser : NSObject {
 @private
  InfoBubbleWindow* window_;  // Weak. Window to close.
}
- (id)initWithWindow:(InfoBubbleWindow*)window;
@end

@implementation InfoBubbleWindowCloser

- (id)initWithWindow:(InfoBubbleWindow*)window {
  if ((self = [super init])) {
    window_ = window;
  }
  return self;
}

// Callback for the alpha animation. Closes window_ if appropriate.
- (void)animationDidStop:(CAAnimation*)anim finished:(BOOL)flag {
  // When alpha reaches zero, close window_.
  if ([window_ alphaValue] == 0.0) {
    [window_ finishCloseAfterAnimation];
  }
}

@end


@implementation InfoBubbleWindow

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)aStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)flag {
  if ((self = [super initWithContentRect:contentRect
                               styleMask:NSBorderlessWindowMask
                                 backing:bufferingType
                                   defer:flag])) {
    [self setBackgroundColor:[NSColor clearColor]];
    [self setExcludedFromWindowsMenu:YES];
    [self setOpaque:NO];
    [self setHasShadow:YES];

    // Start invisible. Will be made visible when ordered front.
    [self setAlphaValue:0.0];

    // Set up alphaValue animation so that self is delegate for the animation.
    // Setting up the delegate is required so that the
    // animationDidStop:finished: callback can be handled.
    // Notice that only the alphaValue Animation is replaced in case
    // superclasses set up animations.
    CAAnimation* alphaAnimation = [CABasicAnimation animation];
    scoped_nsobject<InfoBubbleWindowCloser> delegate(
        [[InfoBubbleWindowCloser alloc] initWithWindow:self]);
    [alphaAnimation setDelegate:delegate];
    NSMutableDictionary* animations =
        [NSMutableDictionary dictionaryWithDictionary:[self animations]];
    [animations setObject:alphaAnimation forKey:@"alphaValue"];
    [self setAnimations:animations];
  }
  return self;
}

// According to
// http://www.cocoabuilder.com/archive/message/cocoa/2006/6/19/165953,
// NSBorderlessWindowMask windows cannot become key or main. In this
// case, this is not a desired behavior. As an example, the bubble could have
// buttons.
- (BOOL)canBecomeKeyWindow {
  return YES;
}

- (void)close {
  // Block the window from receiving events while it fades out.
  closing_ = YES;
  // Apply animations to hide self.
  [NSAnimationContext beginGrouping];
  [[NSAnimationContext currentContext]
      gtm_setDuration:kOrderOutAnimationDuration];
  [[self animator] setAlphaValue:0.0];
  [NSAnimationContext endGrouping];
}

// Called by InfoBubbleWindowCloser when the window is to be really closed
// after the fading animation is complete.
- (void)finishCloseAfterAnimation {
  if (closing_) {
    [super close];
  }
}

// Adds animation for info bubbles being ordered to the front.
- (void)orderWindow:(NSWindowOrderingMode)orderingMode
         relativeTo:(NSInteger)otherWindowNumber {
  // According to the documentation '0' is the otherWindowNumber when the window
  // is ordered front.
  if (orderingMode == NSWindowAbove && otherWindowNumber == 0) {
    // Order self appropriately assuming that its alpha is zero as set up
    // in the designated initializer.
    [super orderWindow:orderingMode relativeTo:otherWindowNumber];

    // Set up frame so it can be adjust down by a few pixels.
    NSRect frame = [self frame];
    NSPoint newOrigin = frame.origin;
    newOrigin.y += kOrderInSlideOffset;
    [self setFrameOrigin:newOrigin];

    // Apply animations to show and move self.
    [NSAnimationContext beginGrouping];
    [[NSAnimationContext currentContext]
        gtm_setDuration:kOrderInAnimationDuration];
    [[self animator] setAlphaValue:1.0];
    [[self animator] setFrame:frame display:YES];
    [NSAnimationContext endGrouping];
  } else {
    [super orderWindow:orderingMode relativeTo:otherWindowNumber];
  }
}

// If the window is currently animating a close, block all UI events to the
// window.
- (void)sendEvent:(NSEvent*)theEvent {
  if (!closing_) {
    [super sendEvent:theEvent];
  }
}

- (BOOL)isClosing {
  return closing_;
}
@end
