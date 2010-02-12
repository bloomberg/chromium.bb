// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/section_separator_view.h"

@interface SectionSeparatorView (PrivateMethods)
- (void)drawGradientRect:(NSRect)rect;
- (void)drawBaseLineRect:(NSRect)rect;
- (void)drawTopLineRect:(NSRect)rect;
@end

@implementation SectionSeparatorView

@synthesize showBaseLine = showBaseLine_;
@synthesize baselineSeparatorColor = baselineSeparatorColor_;
@synthesize showTopLine = showTopLine_;
@synthesize toplineSeparatorColor = toplineSeparatorColor_;

- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setShowBaseLine:YES];
    [self setBaselineSeparatorColor:[NSColor grayColor]];
    [self setShowTopLine:YES];
    [self setToplineSeparatorColor:[NSColor lightGrayColor]];
  }
  return self;
}

- (void)dealloc {
  [baselineSeparatorColor_ release];
  [toplineSeparatorColor_ release];
  [super dealloc];
}

- (void)drawRect:(NSRect)rect {
  NSRect gradientBounds = [self bounds];
  NSRect baselineRect = gradientBounds;
  NSRect toplineRect = gradientBounds;
  gradientBounds.size.height -= 1;
  gradientBounds.origin.y += 1;
  baselineRect.size.height = 1;
  baselineRect.origin.y = 0;
  toplineRect.size.height = 1;
  toplineRect.origin.y = gradientBounds.size.height;
  [self drawGradientRect:gradientBounds];
  if ([self showBaseLine])
    [self drawBaseLineRect:baselineRect];
  if ([self showTopLine])
    [self drawTopLineRect:toplineRect];
}

@end

@implementation SectionSeparatorView (PrivateMethods)

// This method draws the gradient fill of the "separator" bar.  The input
// |rect| designates the bounds that will be filled with the the gradient.
// The gradient has two stops, lighter gray blending to
// darker gray, descending from the top of the |rect| to the bottom.
- (void)drawGradientRect:(NSRect)rect {
  // Compute start and end points where to draw the gradient.
  CGPoint startPoint = CGPointMake(NSMinX(rect), NSMinY(rect));
  CGPoint endPoint = CGPointMake(NSMinX(rect), NSMaxY(rect));

  // Setup the context and colorspace.
  CGContextRef context =
      (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
  CGContextSaveGState(context);
  CGColorSpaceRef colorspace =
      CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);

  // Create the gradient.
  const size_t stopCount = 2;
  CGFloat stops[stopCount] = { 0.0, 1.0 };
  CGFloat components[8] = {
      0.75, 0.75, 0.75, 1.0,   // start color
      0.95, 0.95, 0.95, 1.0 }; // end color

  CGGradientRef gradient = CGGradientCreateWithColorComponents(
      colorspace, components, stops, stopCount);

  CGContextClipToRect(context, *(CGRect*)&rect);
  CGContextDrawLinearGradient(context, gradient, startPoint, endPoint, 0);

  CGGradientRelease(gradient);
  CGColorSpaceRelease(colorspace);
  CGContextRestoreGState(context);
}

// Draws the base line of the separator bar using the |baselineSeparatorColor_|
// designated color.
- (void)drawBaseLineRect:(NSRect)rect {
  [baselineSeparatorColor_ set];
  NSFrameRect(rect);
}

// Draws the top line of the separator bar using the |toplineSeparatorColor_|
// designated color.
- (void)drawTopLineRect:(NSRect)rect {
  [toplineSeparatorColor_ set];
  NSFrameRect(rect);
}

@end
