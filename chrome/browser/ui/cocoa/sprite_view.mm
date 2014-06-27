// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/sprite_view.h"

#import <QuartzCore/CAAnimation.h>
#import <QuartzCore/CATransaction.h>

#include "base/logging.h"
#include "ui/base/cocoa/animation_utils.h"

static const CGFloat kFrameDuration = 0.03;  // 30ms for each animation frame.

@implementation SpriteView

- (instancetype)initWithFrame:(NSRect)frame {
  if (self = [super initWithFrame:frame]) {
    // A layer-hosting view. It will clip its sublayers,
    // if they exceed the boundary.
    CALayer* layer = [CALayer layer];
    layer.masksToBounds = YES;

    imageLayer_ = [CALayer layer];
    imageLayer_.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;

    [layer addSublayer:imageLayer_];
    imageLayer_.frame = layer.bounds;
    [layer setDelegate:self];
    [self setLayer:layer];
    [self setWantsLayer:YES];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)viewWillMoveToWindow:(NSWindow*)newWindow {
  if ([self window]) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:self
                  name:NSWindowWillMiniaturizeNotification
                object:[self window]];
    [[NSNotificationCenter defaultCenter]
        removeObserver:self
                  name:NSWindowDidDeminiaturizeNotification
                object:[self window]];
  }

  if (newWindow) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(updateAnimation:)
               name:NSWindowWillMiniaturizeNotification
             object:newWindow];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(updateAnimation:)
               name:NSWindowDidDeminiaturizeNotification
             object:newWindow];
  }
}

- (void)viewDidMoveToWindow {
  [self updateAnimation:nil];
}

- (void)updateAnimation:(NSNotification*)notification {
  if (spriteAnimation_.get()) {
    // Only animate the sprites if we are attached to a window, and that window
    // is not currently minimized or in the middle of a minimize animation.
    // http://crbug.com/350329
    if ([self window] && ![[self window] isMiniaturized]) {
      if ([imageLayer_ animationForKey:[spriteAnimation_ keyPath]] == nil)
        [imageLayer_ addAnimation:spriteAnimation_.get()
                     forKey:[spriteAnimation_ keyPath]];
    } else {
      [imageLayer_ removeAnimationForKey:[spriteAnimation_ keyPath]];
    }
  }
}

- (void)setImage:(NSImage*)image {
  ScopedCAActionDisabler disabler;

  if (spriteAnimation_.get()) {
    [imageLayer_ removeAnimationForKey:[spriteAnimation_ keyPath]];
    spriteAnimation_.reset();
  }

  [imageLayer_ setContents:image];

  if (image != nil) {
    NSSize imageSize = [image size];
    NSSize spriteSize = NSMakeSize(imageSize.height, imageSize.height);
    [self setFrameSize:spriteSize];

    const NSUInteger spriteCount = imageSize.width / spriteSize.width;
    const CGFloat unitWidth = 1.0 / spriteCount;

    // Show the first (leftmost) sprite.
    [imageLayer_ setContentsRect:CGRectMake(0, 0, unitWidth, 1.0)];

    if (spriteCount > 1) {
      // Animate the sprite offsets, we use a keyframe animation with discrete
      // calculation mode to prevent interpolation.
      NSMutableArray* xOffsets = [NSMutableArray arrayWithCapacity:spriteCount];
      for (NSUInteger i = 0; i < spriteCount; ++i) {
        [xOffsets addObject:@(i * unitWidth)];
      }
      CAKeyframeAnimation* animation =
          [CAKeyframeAnimation animationWithKeyPath:@"contentsRect.origin.x"];
      [animation setValues:xOffsets];
      [animation setCalculationMode:kCAAnimationDiscrete];
      [animation setRepeatCount:HUGE_VALF];
      [animation setDuration:kFrameDuration * [xOffsets count]];
      spriteAnimation_.reset([animation retain]);

      [self updateAnimation:nil];
    }
  }
}

- (void)setImage:(NSImage*)image withToastAnimation:(BOOL)animate {
  if (!animate || [imageLayer_ contents] == nil) {
    [self setImage:image];
  } else {
    // Animate away the icon.
    CABasicAnimation* animation =
        [CABasicAnimation animationWithKeyPath:@"position.y"];
    CGFloat height = CGRectGetHeight([imageLayer_ bounds]);
    [animation setToValue:@(-height)];
    [animation setDuration:kFrameDuration * height];

    // Don't remove on completion to prevent the presentation layer from
    // snapping back to the model layer's value.
    // It will instead be removed when we add the return animation because they
    // have the same key.
    [animation setRemovedOnCompletion:NO];
    [animation setFillMode:kCAFillModeForwards];

    [CATransaction begin];
    [CATransaction setCompletionBlock:^{
        // At the end of the animation, change to the new image and animate
        // it back to position.
        [self setImage:image];

        CABasicAnimation* reverseAnimation =
            [CABasicAnimation animationWithKeyPath:[animation keyPath]];
        [reverseAnimation setFromValue:[animation toValue]];
        [reverseAnimation setToValue:[animation fromValue]];
        [reverseAnimation setDuration:[animation duration]];
        [imageLayer_ addAnimation:reverseAnimation forKey:@"position"];
    }];
    [imageLayer_ addAnimation:animation forKey:@"position"];
    [CATransaction commit];
  }
}

- (BOOL)layer:(CALayer*)layer
    shouldInheritContentsScale:(CGFloat)scale
                    fromWindow:(NSWindow*)window {
  return YES;
}

@end
