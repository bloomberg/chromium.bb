// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/download_shelf_view.h"

#include "base/scoped_nsobject.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

@implementation DownloadShelfView

- (NSColor*)strokeColor {
  return [[self gtm_theme] strokeColorForStyle:GTMThemeStyleToolBar
                                         state:[[self window] isKeyWindow]];
}

- (void)drawRect:(NSRect)rect {
  BOOL isKey = [[self window] isKeyWindow];

  GTMTheme *theme = [self gtm_theme];

  NSImage *backgroundImage = [theme backgroundImageForStyle:GTMThemeStyleToolBar
                                               state:GTMThemeStateActiveWindow];
  if (backgroundImage) {
    [[NSGraphicsContext currentContext] setPatternPhase:NSZeroPoint];
    NSColor *color = [NSColor colorWithPatternImage:backgroundImage];
    [color set];
    NSRectFill([self bounds]);
  } else {
    NSGradient *gradient = [theme gradientForStyle:GTMThemeStyleToolBar
                                             state:isKey];
    NSPoint startPoint = [self convertPointFromBase:NSMakePoint(0, 0)];
    NSPoint endPoint = [self convertPointFromBase:
        NSMakePoint(0, [self frame].size.height)];

    [gradient drawFromPoint:startPoint
                    toPoint:endPoint
                    options:NSGradientDrawsBeforeStartingLocation |
                            NSGradientDrawsAfterEndingLocation];
  }

  // Draw top stroke
  [[self strokeColor] set];
  NSRect borderRect, contentRect;
  NSDivideRect([self bounds], &borderRect, &contentRect, 1, NSMaxYEdge);
  NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
}

// Mouse down events on the download shelf should not allow dragging the parent
// window around.
- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

@end
