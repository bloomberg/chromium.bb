// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/background_gradient_view.h"

#define kToolbarTopOffset 12
#define kToolbarMaxHeight 100

@implementation BackgroundGradientView
@synthesize showsDivider = showsDivider_;

- (id)initWithFrame:(NSRect)frameRect {
  self = [super initWithFrame:frameRect];
  if (self != nil) {
    showsDivider_ = YES;
  }
  return self;
}

- (void)awakeFromNib {
  showsDivider_ = YES;
}

- (void)setShowsDivider:(BOOL)show {
  showsDivider_ = show;
  [self setNeedsDisplay:YES];
}

// The offset of this pattern to make it line up with the top of the window.
- (NSPoint)patternPhase {
  NSPoint phase = NSZeroPoint;
  phase.y += NSHeight([[self window] frame]) - kToolbarTopOffset;
  return phase;
}

- (void)drawRect:(NSRect)rect {
  BOOL isKey = [[self window] isKeyWindow];

  GTMTheme *theme = [self gtm_theme];

  NSImage *backgroundImage = [theme backgroundImageForStyle:GTMThemeStyleToolBar
                                               state:GTMThemeStateActiveWindow];
  if (backgroundImage) {
    NSPoint phase = [self patternPhase];
    [[NSGraphicsContext currentContext] setPatternPhase:phase];

    NSColor *color = [NSColor colorWithPatternImage:backgroundImage];
    [color set];
    NSRectFill([self bounds]);
  } else {
    CGFloat winHeight = NSHeight([[self window] frame]);
    NSGradient *gradient = [theme gradientForStyle:GTMThemeStyleToolBar
                                             state:isKey];
    NSPoint startPoint =
        [self convertPoint:NSMakePoint(0, winHeight - kToolbarTopOffset)
                  fromView:nil];
    NSPoint endPoint =
        [self convertPoint:NSMakePoint(0, winHeight -
                                            kToolbarTopOffset -
                                            kToolbarMaxHeight)
                  fromView:nil];

    [gradient drawFromPoint:startPoint
                    toPoint:endPoint
                    options:NSGradientDrawsBeforeStartingLocation |
                            NSGradientDrawsAfterEndingLocation];
  }

  if (showsDivider_) {
    // Draw bottom stroke
    [[self strokeColor] set];
    NSRect borderRect, contentRect;
    NSDivideRect([self bounds], &borderRect, &contentRect, 1, NSMinYEdge);
    NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
  }
}

- (NSColor*)strokeColor {
  return [[self gtm_theme] strokeColorForStyle:GTMThemeStyleToolBar
                                         state:[[self window] isKeyWindow]];
}

@end
