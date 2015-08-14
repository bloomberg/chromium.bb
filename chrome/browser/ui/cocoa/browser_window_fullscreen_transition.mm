// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_fullscreen_transition.h"

#include <QuartzCore/QuartzCore.h>

#include "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/framed_browser_window.h"

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

// This class locks and unlocks the FrameBrowserWindow. Its destructor ensures
// that the lock gets released.
class FrameAndStyleLock {
 public:
  explicit FrameAndStyleLock(FramedBrowserWindow* window) : window_(window) {}

  ~FrameAndStyleLock() { set_lock(NO); }

  void set_lock(bool lock) { [window_ setFrameAndStyleMaskLock:lock]; }

 private:
  FramedBrowserWindow* window_;  // weak

  DISALLOW_COPY_AND_ASSIGN(FrameAndStyleLock);
};

}  // namespace

@interface BrowserWindowFullscreenTransition () {
  // Flag to keep track of whether we are entering or exiting fullscreen.
  BOOL isEnteringFullscreen_;

  // The window which is undergoing the fullscreen transition.
  base::scoped_nsobject<FramedBrowserWindow> primaryWindow_;

  // A layer that holds a snapshot of the original state of |primaryWindow_|.
  base::scoped_nsobject<CALayer> snapshotLayer_;

  // A temporary window that holds |snapshotLayer_|.
  base::scoped_nsobject<NSWindow> snapshotWindow_;

  // The background color of |primaryWindow_| before the transition began.
  base::scoped_nsobject<NSColor> primaryWindowInitialBackgroundColor_;

  // Whether |primaryWindow_| was opaque before the transition began.
  BOOL primaryWindowInitialOpaque_;

  // Whether the instance is in the process of changing the size of
  // |primaryWindow_|.
  BOOL changingPrimaryWindowSize_;

  // The frame of the |primaryWindow_| before it starts the transition.
  NSRect initialFrame_;

  // The root layer's frame at the beginning of the exit transition. This ivar
  // holds the value of the expected frame at the end of transition, which is
  // relative to the current screen's coordinate system. At the beginning of
  // the exit fullscreen animation, the root layer of the window is immediately
  // moved and resized to match this frame.
  NSRect exitLayerFrame_;

  // The frame that |primaryWindow_| is expected to have after the transition
  // is finished.
  NSRect finalFrame_;

  // Locks and unlocks the FullSizeContentWindow.
  scoped_ptr<FrameAndStyleLock> lock_;
}

// Takes a snapshot of |primaryWindow_| and puts it in |snapshotLayer_|.
- (void)takeSnapshot;

// Creates |snapshotWindow_| and adds |snapshotLayer_| to it.
- (void)makeAndPrepareSnapshotWindow;

// This method has several effects on |primaryWindow_|:
//  - Saves current state.
//  - Makes window transparent, with clear background.
//  - If we are entering fullscreen, it will also:
//    - Add NSFullScreenWindowMask style mask.
//    - Set the size to the screen's size.
- (void)preparePrimaryWindowForAnimation;

// Applies the fullscreen animation to |snapshotLayer_|.
- (void)animateSnapshotWindowWithDuration:(CGFloat)duration;

// Sets |primaryWindow_|'s frame to the expected frame.
- (void)changePrimaryWindowToFinalFrame;

// Override of CAAnimation delegate method.
- (void)animationDidStop:(CAAnimation*)theAnimation finished:(BOOL)flag;

// Returns the layer of the root view of |window|.
- (CALayer*)rootLayerOfWindow:(NSWindow*)window;

@end

@implementation BrowserWindowFullscreenTransition

// -------------------------Public Methods----------------------------

- (instancetype)initEnterWithWindow:(FramedBrowserWindow*)window {
  DCHECK(window);
  DCHECK([self rootLayerOfWindow:window]);
  if ((self = [super init])) {
    primaryWindow_.reset([window retain]);

    isEnteringFullscreen_ = YES;
    initialFrame_ = [primaryWindow_ frame];
    finalFrame_ = [[primaryWindow_ screen] frame];
  }
  return self;
}

- (instancetype)initExitWithWindow:(FramedBrowserWindow*)window
                             frame:(NSRect)frame {
  DCHECK(window);
  DCHECK([self rootLayerOfWindow:window]);
  if ((self = [super init])) {
    primaryWindow_.reset([window retain]);

    isEnteringFullscreen_ = NO;
    finalFrame_ = frame;
    initialFrame_ = [[primaryWindow_ screen] frame];

    // Calculate the coordinates of the finalFrame, relative to the current
    // screen it's on.
    CGFloat screenOriginX = finalFrame_.origin.x - initialFrame_.origin.x;
    CGFloat screenOriginY = finalFrame_.origin.y - initialFrame_.origin.y;
    exitLayerFrame_ =
        NSMakeRect(screenOriginX, screenOriginY, finalFrame_.size.width,
                   finalFrame_.size.height);

    lock_.reset(new FrameAndStyleLock(window));
  }
  return self;
}

- (NSArray*)customWindowsForFullScreenTransition {
  [self takeSnapshot];
  [self makeAndPrepareSnapshotWindow];
  return @[ primaryWindow_.get(), snapshotWindow_.get() ];
}

- (void)startCustomFullScreenAnimationWithDuration:(NSTimeInterval)duration {
  [self preparePrimaryWindowForAnimation];
  [self animatePrimaryWindowWithDuration:duration];
  [self animateSnapshotWindowWithDuration:duration];
}

- (BOOL)shouldWindowBeUnconstrained {
  return changingPrimaryWindowSize_;
}

- (NSSize)desiredWindowLayoutSize {
  return isEnteringFullscreen_ ? [primaryWindow_ frame].size
                               : [[primaryWindow_ contentView] bounds].size;
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
  CGColorRef colorRef = CGColorCreateGenericRGB(0, 0, 0, 0);
  [snapshotLayer_ setBackgroundColor:colorRef];
  CGColorRelease(colorRef);
}

- (void)makeAndPrepareSnapshotWindow {
  DCHECK(snapshotLayer_);

  snapshotWindow_.reset([[NSWindow alloc]
      initWithContentRect:[[primaryWindow_ screen] frame]
                styleMask:0
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  [[snapshotWindow_ contentView] setWantsLayer:YES];
  [snapshotWindow_ setOpaque:NO];
  [snapshotWindow_ setBackgroundColor:[NSColor clearColor]];
  [snapshotWindow_ setAnimationBehavior:NSWindowAnimationBehaviorNone];

  [[[snapshotWindow_ contentView] layer] addSublayer:snapshotLayer_];

  // Compute the frame of the snapshot layer such that the snapshot is
  // positioned exactly on top of the original position of |primaryWindow_|.
  NSRect snapshotLayerFrame =
      [snapshotWindow_ convertRectFromScreen:[primaryWindow_ frame]];
  [snapshotLayer_ setFrame:snapshotLayerFrame];
}

- (void)preparePrimaryWindowForAnimation {
  // Save the initial state of the primary window.
  primaryWindowInitialBackgroundColor_.reset(
      [[primaryWindow_ backgroundColor] copy]);
  primaryWindowInitialOpaque_ = [primaryWindow_ isOpaque];

  // Make |primaryWindow_| invisible. This must happen before the window is
  // resized, since resizing the window will call drawRect: and cause content
  // to flash over the entire screen.
  [primaryWindow_ setOpaque:NO];
  CALayer* root = [self rootLayerOfWindow:primaryWindow_];
  root.opacity = 0;

  if (isEnteringFullscreen_) {
    // As soon as the style mask includes the flag NSFullScreenWindowMask, the
    // window is expected to receive fullscreen layout. This must be set before
    // the window is resized, as that causes a relayout.
    [primaryWindow_
        setStyleMask:[primaryWindow_ styleMask] | NSFullScreenWindowMask];
    [self changePrimaryWindowToFinalFrame];
  } else {
    // Right before the animation begins, change the contentView size to the
    // expected size at the end of the animation. Afterwards, lock the
    // |primaryWindow_| so that AppKit will not be able to make unwanted
    // changes to it during the animation.
    root.frame = exitLayerFrame_;
    [primaryWindow_ forceContentViewSize:finalFrame_.size];
    lock_->set_lock(YES);
  }
}

- (void)animateSnapshotWindowWithDuration:(CGFloat)duration {
  [snapshotWindow_ orderFront:nil];

  // Calculate the frame so that it's relative to the screen.
  NSRect finalFrameRelativeToScreen =
      [snapshotWindow_ convertRectFromScreen:finalFrame_];

  // Move the snapshot layer until it's bottom-left corner is at the the
  // bottom-left corner of the expected frame.
  CABasicAnimation* positionAnimation =
      [CABasicAnimation animationWithKeyPath:@"position"];
  positionAnimation.toValue =
      [NSValue valueWithPoint:finalFrameRelativeToScreen.origin];
  positionAnimation.timingFunction = [CAMediaTimingFunction
      functionWithName:TransformAnimationTimingFunction()];

  // Resize the bounds until it reaches the expected size at the end of the
  // animation.
  NSRect finalBounds =
      NSMakeRect(0, 0, NSWidth(finalFrame_), NSHeight(finalFrame_));
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
  NSRect initialFrame = initialFrame_;
  NSRect endFrame = finalFrame_;
  CGFloat xScale = NSWidth(initialFrame) / NSWidth(endFrame);
  CGFloat yScale = NSHeight(initialFrame) / NSHeight(endFrame);
  CATransform3D initial = CATransform3DMakeScale(xScale, yScale, 1);
  CABasicAnimation* transformAnimation =
      [CABasicAnimation animationWithKeyPath:@"transform"];
  transformAnimation.fromValue = [NSValue valueWithCATransform3D:initial];

  // Calculate the initial position of the root layer. This calculation is
  // agnostic of the anchorPoint.
  CALayer* root = [self rootLayerOfWindow:primaryWindow_];
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
      @[ opacityAnimation, positionAnimation, transformAnimation ];
  group.timingFunction = [CAMediaTimingFunction
      functionWithName:TransformAnimationTimingFunction()];
  group.duration = duration;
  [group setValue:kPrimaryWindowAnimationID forKey:kAnimationIDKey];
  group.delegate = self;

  [root addAnimation:group forKey:kPrimaryWindowAnimationID];
}

- (void)changePrimaryWindowToFinalFrame {
  changingPrimaryWindowSize_ = YES;
  [primaryWindow_ setFrame:finalFrame_ display:NO];
  changingPrimaryWindowSize_ = NO;
}

- (void)animationDidStop:(CAAnimation*)theAnimation finished:(BOOL)flag {
  NSString* animationID = [theAnimation valueForKey:kAnimationIDKey];

  // Remove the snapshot window.
  if ([animationID isEqual:kSnapshotWindowAnimationID]) {
    [snapshotWindow_ orderOut:nil];
    snapshotWindow_.reset();
    snapshotLayer_.reset();
    return;
  }

  if ([animationID isEqual:kPrimaryWindowAnimationID]) {
    // If we're exiting full screen, we want to set the |primaryWindow_|'s frame
    // to the expected frame at the end of the animation. The window's lock
    // must also be released.
    if (!isEnteringFullscreen_) {
      lock_->set_lock(NO);
      [primaryWindow_
          setStyleMask:[primaryWindow_ styleMask] & ~NSFullScreenWindowMask];
      [self changePrimaryWindowToFinalFrame];
    }

    // Checks if the contentView size is correct.
    NSSize expectedSize = finalFrame_.size;
    NSView* content = [primaryWindow_ contentView];
    DCHECK_EQ(NSHeight(content.frame), expectedSize.height);
    DCHECK_EQ(NSWidth(content.frame), expectedSize.width);

    // Restore the state of the primary window and make it visible again.
    [primaryWindow_ setOpaque:primaryWindowInitialOpaque_];
    [primaryWindow_ setBackgroundColor:primaryWindowInitialBackgroundColor_];

    CALayer* root = [self rootLayerOfWindow:primaryWindow_];
    [root removeAnimationForKey:kPrimaryWindowAnimationID];
    root.opacity = 1;
  }
}

- (CALayer*)rootLayerOfWindow:(NSWindow*)window {
  return [[[window contentView] superview] layer];
}

@end
