// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/hover_close_button.h"

#include "app/l10n_util.h"
#include "base/scoped_nsobject.h"
#include "grit/generated_resources.h"
#import "third_party/molokocacao/NSBezierPath+MCAdditions.h"

namespace  {
// Convenience function to return the middle point of the given |rect|.
static NSPoint MidRect(NSRect rect) {
  return NSMakePoint(NSMidX(rect), NSMidY(rect));
}

const CGFloat kCircleRadiusPercentage = 0.415;
const CGFloat kCircleHoverWhite = 0.565;
const CGFloat kCircleClickWhite = 0.396;
const CGFloat kXShadowAlpha = 0.75;
const CGFloat kXShadowCircleAlpha = 0.1;
}  // namespace

@interface HoverCloseButton(Private)
- (void)setUpDrawingPaths;
@end

@implementation HoverCloseButton

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    [self commonInit];
  }
  return self;
}

- (void)awakeFromNib {
  [self commonInit];
}

- (void)drawRect:(NSRect)rect {
  if (!circlePath_.get() || !xPath_.get())
    [self setUpDrawingPaths];

  // If the user is hovering over the button, a light/dark gray circle is drawn
  // behind the 'x'.
  if (hoverState_ != kHoverStateNone) {
    // Adjust the darkness of the circle depending on whether it is being
    // clicked.
    CGFloat white = (hoverState_ == kHoverStateMouseOver) ?
        kCircleHoverWhite : kCircleClickWhite;
    [[NSColor colorWithCalibratedWhite:white alpha:1.0] set];
    [circlePath_ fill];
  }

  [[NSColor whiteColor] set];
  [xPath_ fill];

  // Give the 'x' an inner shadow for depth. If the button is in a hover state
  // (circle behind it), then adjust the shadow accordingly (not as harsh).
  NSShadow* shadow = [[[NSShadow alloc] init] autorelease];
  CGFloat alpha = (hoverState_ != kHoverStateNone) ?
      kXShadowCircleAlpha : kXShadowAlpha;
  [shadow setShadowColor:[NSColor colorWithCalibratedWhite:0.15
                                                     alpha:alpha]];
  [shadow setShadowOffset:NSMakeSize(0.0, 0.0)];
  [shadow setShadowBlurRadius:2.5];
  [xPath_ fillWithInnerShadow:shadow];
}

- (void)commonInit {
  [self setTrackingEnabled:YES];
  hoverState_ = kHoverStateNone;
  [self updateTrackingAreas];

  // Set accessibility description.
  NSString* description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_CLOSE);
  [[self cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
}

- (void)setUpDrawingPaths {
  NSPoint viewCenter = MidRect([self bounds]);

  circlePath_.reset([[NSBezierPath bezierPath] retain]);
  [circlePath_ moveToPoint:viewCenter];
  CGFloat radius = kCircleRadiusPercentage * NSWidth([self bounds]);
  [circlePath_ appendBezierPathWithArcWithCenter:viewCenter
                                          radius:radius
                                      startAngle:0.0
                                        endAngle:365.0];

  // Construct an 'x' by drawing two intersecting rectangles in the shape of a
  // cross and then rotating the path by 45 degrees.
  xPath_.reset([[NSBezierPath bezierPath] retain]);
  [xPath_ appendBezierPathWithRect:NSMakeRect(3.5, 7.0, 9.0, 2.0)];
  [xPath_ appendBezierPathWithRect:NSMakeRect(7.0, 3.5, 2.0, 9.0)];

  NSPoint pathCenter = MidRect([xPath_ bounds]);

  NSAffineTransform* transform = [NSAffineTransform transform];
  [transform translateXBy:viewCenter.x yBy:viewCenter.y];
  [transform rotateByDegrees:45.0];
  [transform translateXBy:-pathCenter.x yBy:-pathCenter.y];

  [xPath_ transformUsingAffineTransform:transform];
}

- (void)dealloc {
  [self setTrackingEnabled:NO];
  [super dealloc];
}

- (void)mouseEntered:(NSEvent*)theEvent {
  hoverState_ = kHoverStateMouseOver;
  [self setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent*)theEvent {
  hoverState_ = kHoverStateNone;
  [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent*)theEvent {
  hoverState_ = kHoverStateMouseDown;
  [self setNeedsDisplay:YES];
  // The hover button needs to hold onto itself here for a bit.  Otherwise,
  // it can be freed while |super mouseDown:| is in it's loop, and the
  // |checkImageState| call will crash.
  // http://crbug.com/28220
  scoped_nsobject<HoverCloseButton> myself([self retain]);

  [super mouseDown:theEvent];
  // We need to check the image state after the mouseDown event loop finishes.
  // It's possible that we won't get a mouseExited event if the button was
  // moved under the mouse during tab resize, instead of the mouse moving over
  // the button.
  // http://crbug.com/31279
  [self checkImageState];
}

- (void)setTrackingEnabled:(BOOL)enabled {
  if (enabled) {
    closeTrackingArea_.reset(
        [[NSTrackingArea alloc] initWithRect:[self bounds]
                                     options:NSTrackingMouseEnteredAndExited |
                                             NSTrackingActiveAlways
                                       owner:self
                                    userInfo:nil]);
    [self addTrackingArea:closeTrackingArea_.get()];

    // If you have a separate window that overlaps the close button, and you
    // move the mouse directly over the close button without entering another
    // part of the tab strip, we don't get any mouseEntered event since the
    // tracking area was disabled when we entered.
    [self checkImageState];
  } else {
    if (closeTrackingArea_.get()) {
      [self removeTrackingArea:closeTrackingArea_.get()];
      closeTrackingArea_.reset(nil);
    }
  }
}

- (void)updateTrackingAreas {
  [super updateTrackingAreas];
  [self checkImageState];
}

- (void)checkImageState {
  if (!closeTrackingArea_.get())
    return;

  // Update the close buttons if the tab has moved.
  NSPoint mouseLoc = [[self window] mouseLocationOutsideOfEventStream];
  mouseLoc = [self convertPoint:mouseLoc fromView:nil];
  hoverState_ = NSPointInRect(mouseLoc, [self bounds]) ?
      kHoverStateMouseOver : kHoverStateNone;
  [self setNeedsDisplay:YES];
}

@end
