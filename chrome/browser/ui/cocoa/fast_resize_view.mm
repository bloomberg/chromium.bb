// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fast_resize_view.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "ui/base/cocoa/animation_utils.h"

namespace {

// The radius of the rounded corners.
const CGFloat kRoundedCornerRadius = 4;

}  // namespace

@interface FastResizeView (PrivateMethods)
// Creates a path whose bottom two corners are rounded.
// Caller takes ownership of the path.
- (CGPathRef)createRoundedBottomCornersPath:(NSSize)size;

// Updates the path of the layer mask to reflect the current value of
// roundedBottomCorners_.
- (void)updateLayerMask;
@end

@implementation FastResizeView

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    ScopedCAActionDisabler disabler;
    base::scoped_nsobject<CALayer> layer([[CALayer alloc] init]);
    [layer setBackgroundColor:CGColorGetConstantColor(kCGColorWhite)];
    [self setLayer:layer];
    [self setWantsLayer:YES];

    roundedBottomCorners_ = YES;
    [self updateLayerMask];
  }
  return self;
}

- (BOOL)isOpaque {
  return YES;
}

- (void)setRoundedBottomCorners:(BOOL)roundedBottomCorners {
  if (roundedBottomCorners == roundedBottomCorners_)
    return;

  roundedBottomCorners_ = roundedBottomCorners;
  [self updateLayerMask];
}

// Every time the frame's size changes, the layer mask needs to be updated.
- (void)setFrameSize:(NSSize)newSize {
  [super setFrameSize:newSize];
  [self updateLayerMask];
}

@end

@implementation FastResizeView (PrivateMethods)

- (CGPathRef)createRoundedBottomCornersPath:(NSSize)size {
  CGMutablePathRef path = CGPathCreateMutable();
  CGFloat width = size.width;
  CGFloat height = size.height;
  CGFloat cornerRadius = kRoundedCornerRadius;

  // Top left corner.
  CGPathMoveToPoint(path, NULL, 0, height);

  // Top right corner.
  CGPathAddLineToPoint(path, NULL, width, height);

  // Bottom right corner.
  CGPathAddArc(path,
               NULL,
               width - cornerRadius,
               cornerRadius,
               cornerRadius,
               0,
               -M_PI_2,
               true);

  // Bottom left corner.
  CGPathAddArc(path,
               NULL,
               cornerRadius,
               cornerRadius,
               cornerRadius,
               -M_PI_2,
               -M_PI,
               true);

  // Draw line back to top-left corner.
  CGPathAddLineToPoint(path, NULL, 0, height);
  CGPathCloseSubpath(path);
  return path;
}

- (void)updateLayerMask {
  if (!roundedBottomCorners_) {
    [self layer].mask = nil;
    layerMask_ = nil;
    return;
  }

  if (![self layer].mask) {
    layerMask_ = [CAShapeLayer layer];
    [self layer].mask = layerMask_;
  }

  CGPathRef path = [self createRoundedBottomCornersPath:self.bounds.size];
  layerMask_.path = path;
  CGPathRelease(path);
}

@end
