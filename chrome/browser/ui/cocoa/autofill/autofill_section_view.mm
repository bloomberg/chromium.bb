// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_section_view.h"

#import "chrome/browser/ui/cocoa/chrome_style.h"
#include "skia/ext/skia_utils_mac.h"

namespace {

// Slight shading for mouse hover.
SkColor kShadingColor = 0x07000000;  // SkColorSetARGB(7, 0, 0, 0);

}  // namespace

@implementation AutofillSectionView

@synthesize clickTarget = clickTarget_;
@synthesize shouldHighlightOnHover = shouldHighlightOnHover_;
@synthesize isHighlighted = isHighlighted_;

- (void)updateHoverState {
  NSPoint mouseLoc = [[self window] mouseLocationOutsideOfEventStream];
  mouseLoc = [self convertPoint:mouseLoc fromView:nil];
  [self setIsHighlighted:NSPointInRect(mouseLoc, [self bounds])];
}

- (void)mouseEvent:(NSEvent*)event {
  if ([event type] == NSMouseExited)
    [self setIsHighlighted:NO];
  else if ([event type] == NSMouseEntered)
    [self setIsHighlighted:YES];
  else if ([event type] == NSLeftMouseDown)
    [clickTarget_ performClick:clickTarget_];
}

- (void)drawRect:(NSRect)dirtyRect {
  if (shouldHighlightOnHover_ && isHighlighted_) {
    [[self hoverColor] set];
    NSRectFill([self bounds]);
  }
}

- (NSColor*)hoverColor {
  // Shading color is specified as a alpha component color, so premultiply.
  NSColor* shadingColor = skia::SkColorToCalibratedNSColor(kShadingColor);
  NSColor* blendedColor = [[[self window] backgroundColor]
      blendedColorWithFraction:[shadingColor alphaComponent]
                       ofColor:shadingColor];
  return [blendedColor colorWithAlphaComponent:1.0];
}

- (void)setShouldHighlightOnHover:(BOOL)shouldHighlight {
  if (shouldHighlight == shouldHighlightOnHover_)
    return;
  shouldHighlightOnHover_ = shouldHighlight;
  [self setNeedsDisplay:YES];
}

- (void)setIsHighlighted:(BOOL)isHighlighted {
  isHighlighted_ = isHighlighted;
  if (shouldHighlightOnHover_)
    [self setNeedsDisplay:YES];
}

@end
