// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/infobars/infobar_gradient_view.h"

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_container_controller.h"
#import "chrome/browser/ui/cocoa/themed_window.h"

namespace {

const double kBackgroundColorTop[3] =
    {255.0 / 255.0, 242.0 / 255.0, 183.0 / 255.0};
const double kBackgroundColorBottom[3] =
    {250.0 / 255.0, 230.0 / 255.0, 145.0 / 255.0};
}

@implementation InfoBarGradientView

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    NSColor* startingColor =
        [NSColor colorWithCalibratedRed:kBackgroundColorTop[0]
                                  green:kBackgroundColorTop[1]
                                   blue:kBackgroundColorTop[2]
                                  alpha:1.0];
    NSColor* endingColor =
        [NSColor colorWithCalibratedRed:kBackgroundColorBottom[0]
                                  green:kBackgroundColorBottom[1]
                                   blue:kBackgroundColorBottom[2]
                                  alpha:1.0];
    scoped_nsobject<NSGradient> gradient(
        [[NSGradient alloc] initWithStartingColor:startingColor
                                       endingColor:endingColor]);
    [self setGradient:gradient];
  }
  return self;
}

- (NSColor*)strokeColor {
  ui::ThemeProvider* themeProvider = [[self window] themeProvider];
  if (!themeProvider)
    return [NSColor blackColor];

  BOOL active = [[self window] isMainWindow];
  return themeProvider->GetNSColor(
      active ? ThemeService::COLOR_TOOLBAR_STROKE :
               ThemeService::COLOR_TOOLBAR_STROKE_INACTIVE,
      true);
}

- (void)drawRect:(NSRect)rect {
  NSRect bounds = [self bounds];
  bounds.size.height = infobars::kBaseHeight;

  // Around the bounds of the infobar, continue drawing the path into which the
  // gradient will be drawn.
  NSBezierPath* infoBarPath = [NSBezierPath bezierPath];
  [infoBarPath moveToPoint:NSMakePoint(NSMinX(bounds), NSMaxY(bounds))];
  [infoBarPath lineToPoint:NSMakePoint(NSMaxX(bounds), NSMaxY(bounds))];
  scoped_nsobject<NSBezierPath> topPath([infoBarPath copy]);

  [infoBarPath lineToPoint:NSMakePoint(NSMaxX(bounds), NSMinY(bounds))];
  [infoBarPath lineToPoint:NSMakePoint(NSMinX(bounds), NSMinY(bounds))];
  [infoBarPath closePath];

  // Draw the gradient.
  [[self gradient] drawInBezierPath:infoBarPath angle:270];

  // Stroke the bottom.
  NSColor* strokeColor = [self strokeColor];
  if (strokeColor) {
    [strokeColor set];
    NSRect borderRect, contentRect;
    NSDivideRect(bounds, &borderRect, &contentRect, 1, NSMinYEdge);
    NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
  }

  // Add an inner stroke.
  [[NSColor colorWithDeviceWhite:1.0 alpha:1.0] setStroke];
  NSAffineTransform* transform = [NSAffineTransform transform];
  [transform translateXBy:0.0 yBy:-1.0];
  [topPath transformUsingAffineTransform:transform];
  [topPath stroke];
}

- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

// This view is intentionally not opaque because it overlaps with the findbar.

- (BOOL)accessibilityIsIgnored {
  return NO;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([attribute isEqual:NSAccessibilityRoleAttribute])
    return NSAccessibilityGroupRole;

  return [super accessibilityAttributeValue:attribute];
}

@end
