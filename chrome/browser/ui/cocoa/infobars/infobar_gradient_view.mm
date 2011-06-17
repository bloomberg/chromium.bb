// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/infobars/infobar_gradient_view.h"

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/tab_contents/infobar.h"
#import "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_container_controller.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_tip_drawing_model.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "skia/ext/skia_utils_mac.h"

@implementation InfoBarGradientView

@synthesize drawingModel = drawingModel_;

- (void)setInfobarType:(InfoBarDelegate::Type)infobarType {
  SkColor topColor = GetInfoBarTopColor(infobarType);
  SkColor bottomColor = GetInfoBarBottomColor(infobarType);
  scoped_nsobject<NSGradient> gradient([[NSGradient alloc]
      initWithStartingColor:gfx::SkColorToCalibratedNSColor(topColor)
                endingColor:gfx::SkColorToCalibratedNSColor(bottomColor)]);
  [self setGradient:gradient];
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
  NSRect baseBounds = [self bounds];
  baseBounds.size.height = infobars::kBaseHeight;

  // Draw the tip that acts as the anti-spoofing countermeasure. This can be
  // NULL when closing.
  scoped_ptr<infobars::Tip> tip([drawingModel_ createTipForView:self]);

  // Around the bounds of the infobar, continue drawing the path into which the
  // gradient will be drawn.
  NSBezierPath* infoBarPath = [NSBezierPath bezierPath];
  [infoBarPath moveToPoint:NSMakePoint(NSMinX(baseBounds), NSMaxY(baseBounds))];
  if (tip.get()) {
    [infoBarPath lineToPoint:NSMakePoint(tip->point.x, NSMaxY(baseBounds))];
    // When animating, this view is full-height, but its superview, the
    // AnimatableView has its height change. Use the MaxY of the superview to
    // determine the Y position of the tip apex so that the tip looks like it
    // grows upwards.
    NSPoint midPoint = tip->mid_point;
    midPoint.y = std::min(midPoint.y, NSMaxY([[self superview] bounds]));
    [tip->path setAssociatedPoints:&midPoint atIndex:1];
    [infoBarPath appendBezierPath:tip->path];
  }
  [infoBarPath lineToPoint:NSMakePoint(NSMaxX(baseBounds), NSMaxY(baseBounds))];
  scoped_nsobject<NSBezierPath> topPath([infoBarPath copy]);

  [infoBarPath lineToPoint:NSMakePoint(NSMaxX(baseBounds), NSMinY(baseBounds))];
  [infoBarPath lineToPoint:NSMakePoint(NSMinX(baseBounds), NSMinY(baseBounds))];
  [infoBarPath lineToPoint:NSMakePoint(NSMinX(baseBounds), NSMaxY(baseBounds))];
  [infoBarPath closePath];

  // Draw the gradient.
  [[self gradient] drawInBezierPath:infoBarPath angle:270];

  // Stroke the bottom.
  NSColor* strokeColor = [self strokeColor];
  if (strokeColor) {
    [strokeColor set];
    NSRect borderRect, contentRect;
    NSDivideRect(baseBounds, &borderRect, &contentRect, 1, NSMinYEdge);
    NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
  }

  // Add an inner stroke.
  [[NSColor colorWithDeviceWhite:1.0 alpha:1.0] setStroke];
  NSAffineTransform* transform = [NSAffineTransform transform];
  [transform translateXBy:0.0 yBy:-1.0];
  [topPath transformUsingAffineTransform:transform];
  [topPath stroke];

  // Stroke the tip.
  if (tip.get()) {
    [tip->path setLineCapStyle:NSSquareLineCapStyle];
    [tip->path setLineJoinStyle:NSMiterLineJoinStyle];
    [[self strokeColor] set];
    [tip->path stroke];
  }
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
