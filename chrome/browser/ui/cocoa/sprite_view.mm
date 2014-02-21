// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/sprite_view.h"

#import <QuartzCore/CAAnimation.h>

#include "base/logging.h"

@implementation SpriteView

- (instancetype)initWithImage:(NSImage*)image {
  return [self initWithImage:image width:[image size].height duration:0.03];
}

- (instancetype)initWithImage:(NSImage*)image
                        width:(CGFloat)spriteWidth
                     duration:(CGFloat)duration {
  DCHECK(image != nil);
  DCHECK(spriteWidth > 0.0);

  NSSize imageSize = [image size];
  CGFloat spriteHeight = imageSize.height;
  DCHECK(spriteWidth > 0.0 && spriteHeight > 0.0);
  if (spriteWidth == 0.0 || spriteHeight == 0.0)
    return nil;

  if (self =
          [super initWithFrame:NSMakeRect(0, 0, spriteWidth, spriteHeight)]) {
    const NSUInteger spriteCount = imageSize.width / spriteWidth;
    const CGFloat unitWidth = 1.0 / spriteCount;

    // A layer-hosting view.
    CALayer* layer = [CALayer layer];
    [layer setDelegate:self];
    [self setLayer:layer];
    [self setWantsLayer:YES];

    // Show the first (leftmost) sprite.
    [layer setContents:image];
    [layer setContentsRect:CGRectMake(0, 0, unitWidth, 1.0)];

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
    [animation setDuration:duration * [xOffsets count]];
    [layer addAnimation:animation forKey:nil];
  }

  return self;
}

- (BOOL)layer:(CALayer*)layer
    shouldInheritContentsScale:(CGFloat)scale
                    fromWindow:(NSWindow*)window {
  return YES;
}

@end
