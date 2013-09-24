// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/tab_projecting_image_view.h"

#include "ui/gfx/animation/animation.h"

@implementation TabProjectingImageView

- (id)initWithFrame:(NSRect)rect
    backgroundImage:(NSImage*)backgroundImage
     projectorImage:(NSImage*)projectorImage
         throbImage:(NSImage*)throbImage
    animationContainer:(gfx::AnimationContainer*)animationContainer {
  if ((self = [super initWithFrame:rect
                   backgroundImage:backgroundImage
                        throbImage:throbImage
                     throbPosition:kThrobPositionOverlay
                animationContainer:animationContainer])) {
    projectorImage_.reset([projectorImage retain]);
  }
  return self;
}

- (void)drawRect:(NSRect)rect {
  // For projecting mode, we need to draw 3 centered icons of different sizes:
  // - glow: 32x32
  // - projection sheet: 16x16
  // - favicon: 12x12 (0.75*16)
  // Our bounds should be set to 32x32.
  NSRect bounds = [self bounds];

  const int faviconWidthAndHeight = bounds.size.width / 2 * 0.75;
  const int faviconX = (bounds.size.width - faviconWidthAndHeight) / 2;
  // Adjustment in y direction because projector screen is thinner at top.
  const int faviconY = faviconX + 1;
  [backgroundImage_ drawInRect:NSMakeRect(faviconX,
                                          faviconY,
                                          faviconWidthAndHeight,
                                          faviconWidthAndHeight)
                      fromRect:NSZeroRect
                     operation:NSCompositeSourceOver
                      fraction:1];

  const int projectorWidthAndHeight = bounds.size.width / 2;
  const int projectorXY = (bounds.size.width - projectorWidthAndHeight) / 2;
  [projectorImage_ drawInRect:NSMakeRect(projectorXY,
                                         projectorXY,
                                         projectorWidthAndHeight,
                                         projectorWidthAndHeight)
                     fromRect:NSZeroRect
                    operation:NSCompositeSourceOver
                     fraction:1];

  [throbImage_ drawInRect:[self bounds]
                 fromRect:NSZeroRect
                operation:NSCompositeSourceOver
                 fraction:throbAnimation_->GetCurrentValue()];
}

@end
