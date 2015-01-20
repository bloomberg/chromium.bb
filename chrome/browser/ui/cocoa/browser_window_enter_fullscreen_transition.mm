// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_enter_fullscreen_transition.h"

#include <QuartzCore/QuartzCore.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"

namespace {

NSString* const kPrimaryWindowAnimationID = @"PrimaryWindowAnimationID";
NSString* const kSnapshotWindowAnimationID = @"SnapshotWindowAnimationID";
NSString* const kAnimationIDKey = @"AnimationIDKey";

// This class has two simultaneous animations to resize and reposition layers.
// These animations must use the same timing function, otherwise there will be
// visual discordance.
NSString* TransformAnimationTimingFunction() {
  return kCAMediaTimingFunctionEaseInEaseOut;
}

}  // namespace

@interface BrowserWindowEnterFullscreenTransition () {
  // The window which is undergoing the fullscreen transition.
  base::scoped_nsobject<NSWindow> primaryWindow_;

  // A layer that holds a snapshot of the original state of |primaryWindow_|.
  base::scoped_nsobject<CALayer> snapshotLayer_;

  // A temporary window that holds |snapshotLayer_|.
  base::scoped_nsobject<NSWindow> snapshotWindow_;

  // The animation applied to |snapshotLayer_|.
  base::scoped_nsobject<CAAnimationGroup> snapshotAnimation_;

  // The animation applied to the root layer of |primaryWindow_|.
  base::scoped_nsobject<CAAnimationGroup> primaryWindowAnimation_;

  // The frame of the |primaryWindow_| before the transition began.
  NSRect primaryWindowInitialFrame_;

  // The background color of |primaryWindow_| before the transition began.
  base::scoped_nsobject<NSColor> primaryWindowInitialBackgroundColor_;

  // Whether |primaryWindow_| was opaque before the transition began.
  BOOL primaryWindowInitialOpaque_;

  // Whether the instance is in the process of changing the size of
  // |primaryWindow_|.
  BOOL changingPrimaryWindowSize_;

  // The frame that |primaryWindow_| is expected to have after the transition
  // is finished.
  NSRect primaryWindowFinalFrame_;
}

// Takes a snapshot of |primaryWindow_| and puts it in |snapshotLayer_|.
- (void)takeSnapshot;

// Creates |snapshotWindow_| and adds |snapshotLayer_| to it.
- (void)makeAndPrepareSnapshotWindow;

// This method has several effects on |primaryWindow_|:
//  - Saves current state.
//  - Makes window transparent, with clear background.
//  - Adds NSFullScreenWindowMask style mask.
//  - Sets the size to the screen's size.
- (void)preparePrimaryWindowForAnimation;

// Applies the fullscreen animation to |snapshotLayer_|.
- (void)animateSnapshotWindowWithDuration:(CGFloat)duration;

// Applies the fullscreen animation to the root layer of |primaryWindow_|.
- (void)animatePrimaryWindowWithDuration:(CGFloat)duration;

// Override of CAAnimation delegate method.
- (void)animationDidStop:(CAAnimation*)theAnimation finished:(BOOL)flag;

// Returns the layer of the root view of |window|.
- (CALayer*)rootLayerOfWindow:(NSWindow*)window;

@end

@implementation BrowserWindowEnterFullscreenTransition

// -------------------------Public Methods----------------------------

- (instancetype)initWithWindow:(NSWindow*)window {
  DCHECK(window);
  DCHECK([self rootLayerOfWindow:window]);
  if ((self = [super init])) {
    primaryWindow_.reset([window retain]);
  }
  return self;
}

- (void)dealloc {
  [snapshotAnimation_ setDelegate:nil];
  [primaryWindowAnimation_ setDelegate:nil];
  [super dealloc];
}

- (NSArray*)customWindowsToEnterFullScreen {
  [self takeSnapshot];
  [self makeAndPrepareSnapshotWindow];
  [self preparePrimaryWindowForAnimation];
  return @[ primaryWindow_.get(), snapshotWindow_.get() ];
}

- (void)startCustomAnimationToEnterFullScreenWithDuration:
    (NSTimeInterval)duration {
  [self animateSnapshotWindowWithDuration:duration];
  [self animatePrimaryWindowWithDuration:duration];
}

- (BOOL)shouldWindowBeUnconstrained {
  return changingPrimaryWindowSize_;
}

// -------------------------Private Methods----------------------------

- (void)takeSnapshot {
  base::ScopedCFTypeRef<CGImageRef> windowSnapshot(CGWindowListCreateImage(
      CGRectNull, kCGWindowListOptionIncludingWindow,
      [primaryWindow_ windowNumber], kCGWindowImageBoundsIgnoreFraming));
  snapshotLayer_.reset([[CALayer alloc] init]);
  [snapshotLayer_ setFrame:NSRectToCGRect([primaryWindow_ frame])];
  [snapshotLayer_ setContents:static_cast<id>(windowSnapshot.get())];
  [snapshotLayer_ setAnchorPoint:CGPointMake(0, 0)];
  [snapshotLayer_ setBackgroundColor:CGColorCreateGenericRGB(0, 0, 0, 0)];
}

- (void)makeAndPrepareSnapshotWindow {
  DCHECK(snapshotLayer_);

  snapshotWindow_.reset(
      [[NSWindow alloc] initWithContentRect:[[primaryWindow_ screen] frame]
                                  styleMask:0
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);
  [[snapshotWindow_ contentView] setWantsLayer:YES];
  [snapshotWindow_ setOpaque:NO];
  [snapshotWindow_ setBackgroundColor:[NSColor clearColor]];
  [snapshotWindow_ setAnimationBehavior:NSWindowAnimationBehaviorNone];

  [snapshotWindow_ orderFront:nil];
  [[[snapshotWindow_ contentView] layer] addSublayer:snapshotLayer_];

  // Compute the frame of the snapshot layer such that the snapshot is
  // positioned exactly on top of the original position of |primaryWindow_|.
  NSRect snapshotLayerFrame =
      [snapshotWindow_ convertRectFromScreen:[primaryWindow_ frame]];
  [snapshotLayer_ setFrame:snapshotLayerFrame];
}

- (void)preparePrimaryWindowForAnimation {
  // Save initial state of |primaryWindow_|.
  primaryWindowInitialFrame_ = [primaryWindow_ frame];
  primaryWindowInitialBackgroundColor_.reset(
      [[primaryWindow_ backgroundColor] copy]);
  primaryWindowInitialOpaque_ = [primaryWindow_ isOpaque];

  primaryWindowFinalFrame_ = [[primaryWindow_ screen] frame];

  // Make |primaryWindow_| invisible. This must happen before the window is
  // resized, since resizing the window will call drawRect: and cause content
  // to flash over the entire screen.
  [primaryWindow_ setOpaque:NO];
  [primaryWindow_ setBackgroundColor:[NSColor clearColor]];
  CALayer* rootLayer = [self rootLayerOfWindow:primaryWindow_];
  rootLayer.opacity = 0;

  // As soon as the style mask includes the flag NSFullScreenWindowMask, the
  // window is expected to receive fullscreen layout. This must be set before
  // the window is resized, as that causes a relayout.
  [primaryWindow_
      setStyleMask:[primaryWindow_ styleMask] | NSFullScreenWindowMask];

  // Resize |primaryWindow_|.
  changingPrimaryWindowSize_ = YES;
  [primaryWindow_ setFrame:primaryWindowFinalFrame_ display:YES];
  changingPrimaryWindowSize_ = NO;
}

- (void)animateSnapshotWindowWithDuration:(CGFloat)duration {
  // Move the snapshot layer until it's bottom-left corner is at the
  // bottom-left corner of the screen.
  CABasicAnimation* positionAnimation =
      [CABasicAnimation animationWithKeyPath:@"position"];
  positionAnimation.toValue = [NSValue valueWithPoint:NSZeroPoint];
  positionAnimation.timingFunction = [CAMediaTimingFunction
      functionWithName:TransformAnimationTimingFunction()];

  // Expand the bounds until it covers the screen.
  NSRect finalBounds = NSMakeRect(0, 0, NSWidth(primaryWindowFinalFrame_),
                                  NSHeight(primaryWindowFinalFrame_));
  CABasicAnimation* boundsAnimation =
      [CABasicAnimation animationWithKeyPath:@"bounds"];
  boundsAnimation.toValue = [NSValue valueWithRect:finalBounds];
  boundsAnimation.timingFunction = [CAMediaTimingFunction
      functionWithName:TransformAnimationTimingFunction()];

  // Fade out the snapshot layer.
  CABasicAnimation* opacityAnimation =
      [CABasicAnimation animationWithKeyPath:@"opacity"];
  opacityAnimation.toValue = @(0.0);
  opacityAnimation.timingFunction =
      [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseIn];

  // Fill forwards, and don't remove the animation. When the animation
  // completes, the entire window will be removed.
  CAAnimationGroup* group = [CAAnimationGroup animation];
  group.removedOnCompletion = NO;
  group.fillMode = kCAFillModeForwards;
  group.animations = @[ positionAnimation, boundsAnimation, opacityAnimation ];
  group.duration = duration;
  [group setValue:kSnapshotWindowAnimationID forKey:kAnimationIDKey];
  group.delegate = self;

  snapshotAnimation_.reset([group retain]);
  [snapshotLayer_ addAnimation:group forKey:nil];
}

- (void)animatePrimaryWindowWithDuration:(CGFloat)duration {
  // As soon as the window's root layer is scaled down, the opacity should be
  // set back to 1. There are a couple of ways to do this. The easiest is to
  // just have a dummy animation as part of the same animation group.
  CABasicAnimation* opacityAnimation =
      [CABasicAnimation animationWithKeyPath:@"opacity"];
  opacityAnimation.fromValue = @(1.0);
  opacityAnimation.toValue = @(1.0);

  // The root layer's size should start scaled down to the initial size of
  // |primaryWindow_|. The animation increases the size until the root layer
  // fills the screen.
  NSRect initialFrame = primaryWindowInitialFrame_;
  NSRect endFrame = primaryWindowFinalFrame_;
  CGFloat xScale = NSWidth(initialFrame) / NSWidth(endFrame);
  CGFloat yScale = NSHeight(initialFrame) / NSHeight(endFrame);
  CATransform3D initial = CATransform3DMakeScale(xScale, yScale, 1);
  CABasicAnimation* transformAnimation =
      [CABasicAnimation animationWithKeyPath:@"transform"];
  transformAnimation.fromValue = [NSValue valueWithCATransform3D:initial];

  CALayer* root = [self rootLayerOfWindow:primaryWindow_];

  // Calculate the initial position of the root layer. This calculation is
  // agnostic of the anchorPoint.
  CGFloat layerStartPositionDeltaX = NSMidX(initialFrame) - NSMidX(endFrame);
  CGFloat layerStartPositionDeltaY = NSMidY(initialFrame) - NSMidY(endFrame);
  NSPoint layerStartPosition =
      NSMakePoint(root.position.x + layerStartPositionDeltaX,
                  root.position.y + layerStartPositionDeltaY);

  // Animate the primary window from its initial position.
  CABasicAnimation* positionAnimation =
      [CABasicAnimation animationWithKeyPath:@"position"];
  positionAnimation.fromValue = [NSValue valueWithPoint:layerStartPosition];

  CAAnimationGroup* group = [CAAnimationGroup animation];
  group.removedOnCompletion = NO;
  group.fillMode = kCAFillModeForwards;
  group.animations =
      @[ transformAnimation, opacityAnimation, positionAnimation ];
  group.timingFunction = [CAMediaTimingFunction
      functionWithName:TransformAnimationTimingFunction()];
  group.duration = duration;
  [group setValue:kPrimaryWindowAnimationID forKey:kAnimationIDKey];
  group.delegate = self;

  primaryWindowAnimation_.reset([group retain]);

  [root addAnimation:group forKey:kPrimaryWindowAnimationID];
}

- (void)animationDidStop:(CAAnimation*)theAnimation finished:(BOOL)flag {
  NSString* animationID = [theAnimation valueForKey:kAnimationIDKey];
  if ([animationID isEqual:kSnapshotWindowAnimationID]) {
    [snapshotWindow_ orderOut:nil];
    snapshotWindow_.reset();
    snapshotLayer_.reset();
    return;
  }

  if ([animationID isEqual:kPrimaryWindowAnimationID]) {
    [primaryWindow_ setOpaque:YES];
    [primaryWindow_ setBackgroundColor:primaryWindowInitialBackgroundColor_];
    CALayer* root = [self rootLayerOfWindow:primaryWindow_];
    root.opacity = 1;
    [root removeAnimationForKey:kPrimaryWindowAnimationID];
  }
}

- (CALayer*)rootLayerOfWindow:(NSWindow*)window {
  return [[[window contentView] superview] layer];
}

@end
