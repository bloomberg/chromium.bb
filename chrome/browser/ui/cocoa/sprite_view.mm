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
    // A layer-hosting view.
    CALayer* layer = [CALayer layer];
    [layer setDelegate:self];
    [self setLayer:layer];
    [self setWantsLayer:YES];
  }
  return self;
}

- (void)setImage:(NSImage*)image {
  ScopedCAActionDisabler disabler;

  CALayer* layer = [self layer];
  [layer removeAnimationForKey:@"contentsRect.origin.x"];
  [layer setContents:image];

  if (image != nil) {
    NSSize imageSize = [image size];
    NSSize spriteSize = NSMakeSize(imageSize.height, imageSize.height);
    [self setFrameSize:spriteSize];

    const NSUInteger spriteCount = imageSize.width / spriteSize.width;
    const CGFloat unitWidth = 1.0 / spriteCount;

    // Show the first (leftmost) sprite.
    [layer setContentsRect:CGRectMake(0, 0, unitWidth, 1.0)];

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
      [layer addAnimation:animation forKey:[animation keyPath]];
    }
  }
}

- (void)setImage:(NSImage*)image withToastAnimation:(BOOL)animate {
  CALayer* layer = [self layer];
  if (!animate || [layer contents] == nil) {
    [self setImage:image];
  } else {
    // Animate away the icon.
    CABasicAnimation* animation =
        [CABasicAnimation animationWithKeyPath:@"position.y"];
    CGFloat height = CGRectGetHeight([layer bounds]);
    [animation setToValue:@(-height)];
    [animation setDuration:kFrameDuration * height];

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
        [layer addAnimation:reverseAnimation forKey:nil];
    }];
    [layer addAnimation:animation forKey:nil];
    [CATransaction commit];
  }
}

- (BOOL)layer:(CALayer*)layer
    shouldInheritContentsScale:(CGFloat)scale
                    fromWindow:(NSWindow*)window {
  return YES;
}

@end
