// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/infobars/infobar_gradient_view.h"

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/themes/theme_properties.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/theme_provider.h"

@implementation InfoBarGradientView

- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  return self;
}

- (id)initWithCoder:(NSCoder*)decoder {
  self = [super initWithCoder:decoder];
  return self;
}

- (void)setInfobarBackgroundColor:(SkColor)color {
  // TODO(ellyjones): no need to use a gradient here.
  base::scoped_nsobject<NSGradient> gradient([[NSGradient alloc]
      initWithStartingColor:skia::SkColorToCalibratedNSColor(color)
                endingColor:skia::SkColorToCalibratedNSColor(color)]);
  [self setGradient:gradient];
}

- (NSColor*)strokeColor {
  const ui::ThemeProvider* themeProvider = [[self window] themeProvider];
  if (!themeProvider)
    return [NSColor blackColor];

  BOOL active = [[self window] isMainWindow];
  return themeProvider->GetNSColor(
      active ? ThemeProperties::COLOR_TOOLBAR_STROKE :
               ThemeProperties::COLOR_TOOLBAR_STROKE_INACTIVE);
}

- (void)drawRect:(NSRect)rect {
  NSRect bounds = [self bounds];

  // Around the bounds of the infobar, continue drawing the path into which the
  // gradient will be drawn.
  NSBezierPath* infoBarPath = [NSBezierPath bezierPath];
  [infoBarPath moveToPoint:NSMakePoint(NSMinX(bounds), NSMaxY(bounds))];
  [infoBarPath lineToPoint:NSMakePoint(NSMaxX(bounds), NSMaxY(bounds))];
  [infoBarPath lineToPoint:NSMakePoint(NSMaxX(bounds), NSMinY(bounds))];
  [infoBarPath lineToPoint:NSMakePoint(NSMinX(bounds), NSMinY(bounds))];
  [infoBarPath closePath];

  // Draw the gradient.
  [[self gradient] drawInBezierPath:infoBarPath angle:270];

  NSColor* strokeColor = [self strokeColor];
  if (strokeColor) {
    [strokeColor set];

    // Stroke the bottom of the infobar.
    NSRect borderRect, contentRect;
    NSDivideRect(bounds, &borderRect, &contentRect, 1, NSMinYEdge);
    NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
  }

  // Add an inner stroke.
  NSBezierPath* highlightPath = [NSBezierPath bezierPath];
  [highlightPath moveToPoint:NSMakePoint(NSMinX(bounds), NSMaxY(bounds) - 1)];
  [highlightPath lineToPoint:NSMakePoint(NSMaxX(bounds), NSMaxY(bounds) - 1)];

  [[NSColor colorWithDeviceWhite:1.0 alpha:1.0] setStroke];
  [highlightPath stroke];
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
