// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fast_resize_view.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/command_line.h"
#include "base/mac/scoped_nsobject.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/ui_base_switches.h"

namespace {

// The radius of the rounded corners.
const CGFloat kRoundedCornerRadius = 4;

}  // namespace

@interface FastResizeView (PrivateMethods)
// Lays out this views subviews.  If fast resize mode is on, does not resize any
// subviews and instead pegs them to the top left.  If fast resize mode is off,
// sets the subviews' frame to be equal to this view's bounds.
- (void)layoutSubviews;

// Creates a path whose bottom two corners are rounded.
// Caller takes ownership of the path.
- (CGPathRef)createRoundedBottomCornersPath:(NSSize)size;

// Updates the path of the layer mask to reflect the current value of
// roundedBottomCorners_ and fastResizeMode_.
- (void)updateLayerMask;
@end

@implementation FastResizeView

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableCoreAnimation)) {
      ScopedCAActionDisabler disabler;
      base::scoped_nsobject<CALayer> layer([[CALayer alloc] init]);
      [layer setBackgroundColor:CGColorGetConstantColor(kCGColorWhite)];
      [self setLayer:layer];
      [self setWantsLayer:YES];

      roundedBottomCorners_ = YES;
      [self updateLayerMask];
    }
  }
  return self;
}

- (BOOL)isOpaque {
  return YES;
}

- (void)setFastResizeMode:(BOOL)fastResizeMode {
  if (fastResizeMode_ == fastResizeMode)
    return;

  fastResizeMode_ = fastResizeMode;
  [self updateLayerMask];

  // Force a relayout when coming out of fast resize mode.
  if (!fastResizeMode_)
    [self layoutSubviews];

  [self setNeedsDisplay:YES];
}

- (void)setRoundedBottomCorners:(BOOL)roundedBottomCorners {
  if (roundedBottomCorners == roundedBottomCorners_)
    return;

  roundedBottomCorners_ = roundedBottomCorners;
  [self updateLayerMask];
}

- (void)resizeSubviewsWithOldSize:(NSSize)oldSize {
  [self layoutSubviews];
}

- (void)drawRect:(NSRect)dirtyRect {
  // If we are in fast resize mode, our subviews may not completely cover our
  // bounds, so we fill with white.  If we are not in fast resize mode, we do
  // not need to draw anything.
  if (!fastResizeMode_)
    return;

  [[NSColor whiteColor] set];
  NSRectFill(dirtyRect);
}

// Every time the frame's size changes, the layer mask needs to be updated.
- (void)setFrameSize:(NSSize)newSize {
  [super setFrameSize:newSize];
  [self updateLayerMask];
}

@end

@implementation FastResizeView (PrivateMethods)

- (void)layoutSubviews {
  // There should never be more than one subview.  There can be zero, if we are
  // in the process of switching tabs or closing the window.  In those cases, no
  // layout is needed.
  NSArray* subviews = [self subviews];
  DCHECK([subviews count] <= 1);
  if ([subviews count] < 1)
    return;

  NSView* subview = [subviews objectAtIndex:0];
  NSRect bounds = [self bounds];

  if (fastResizeMode_) {
    NSRect frame = [subview frame];
    frame.origin.x = 0;
    frame.origin.y = NSHeight(bounds) - NSHeight(frame);
    [subview setFrame:frame];
  } else {
    [subview setFrame:bounds];
  }
}

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
  if (fastResizeMode_ || !roundedBottomCorners_) {
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
