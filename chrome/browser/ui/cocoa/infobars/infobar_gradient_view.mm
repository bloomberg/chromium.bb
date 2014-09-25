// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/infobars/infobar_gradient_view.h"

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/themes/theme_properties.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_container_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "components/infobars/core/infobar.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/theme_provider.h"

@implementation InfoBarGradientView

@synthesize arrowHeight = arrowHeight_;
@synthesize arrowHalfWidth = arrowHalfWidth_;
@synthesize arrowX = arrowX_;
@synthesize hasTip = hasTip_;

- (id)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    hasTip_ = YES;
  }
  return self;
}

- (id)initWithCoder:(NSCoder*)decoder {
  if ((self = [super initWithCoder:decoder])) {
    hasTip_ = YES;
  }
  return self;
}

- (void)setInfobarType:(infobars::InfoBarDelegate::Type)infobarType {
  SkColor topColor = infobars::InfoBar::GetTopColor(infobarType);
  SkColor bottomColor = infobars::InfoBar::GetBottomColor(infobarType);
  base::scoped_nsobject<NSGradient> gradient([[NSGradient alloc]
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
      active ? ThemeProperties::COLOR_TOOLBAR_STROKE :
               ThemeProperties::COLOR_TOOLBAR_STROKE_INACTIVE);
}

- (void)drawRect:(NSRect)rect {
  NSRect bounds = [self bounds];
  bounds.size.height = infobars::InfoBar::kDefaultBarTargetHeight;

  CGFloat tipXOffset = arrowX_ - arrowHalfWidth_;

  // Around the bounds of the infobar, continue drawing the path into which the
  // gradient will be drawn.
  NSBezierPath* infoBarPath = [NSBezierPath bezierPath];
  [infoBarPath moveToPoint:NSMakePoint(NSMinX(bounds), NSMaxY(bounds))];

  // Draw the tip.
  if (hasTip_) {
    [infoBarPath lineToPoint:NSMakePoint(tipXOffset, NSMaxY(bounds))];
    [infoBarPath relativeLineToPoint:NSMakePoint(arrowHalfWidth_,
                                                 arrowHeight_)];
    [infoBarPath relativeLineToPoint:NSMakePoint(arrowHalfWidth_,
                                                 -arrowHeight_)];
  }
  [infoBarPath lineToPoint:NSMakePoint(NSMaxX(bounds), NSMaxY(bounds))];

  // Save off the top path of the infobar.
  base::scoped_nsobject<NSBezierPath> topPath([infoBarPath copy]);

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

    // Re-stroke the top because the tip will have no stroke. This will draw
    // over the divider drawn by the bottom of the tabstrip.
    [topPath stroke];
  }

  // Add an inner stroke.
  const CGFloat kHighlightTipHeight = arrowHeight_ - 1;
  NSBezierPath* highlightPath = [NSBezierPath bezierPath];
  [highlightPath moveToPoint:NSMakePoint(NSMinX(bounds), NSMaxY(bounds) - 1)];
  if (hasTip_) {
    [highlightPath relativeLineToPoint:NSMakePoint(tipXOffset + 1, 0)];
    [highlightPath relativeLineToPoint:NSMakePoint(arrowHalfWidth_ - 1,
                                                   kHighlightTipHeight)];
    [highlightPath relativeLineToPoint:NSMakePoint(arrowHalfWidth_ - 1,
                                                   -kHighlightTipHeight)];
  }
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

- (void)setHasTip:(BOOL)hasTip {
  if (hasTip_ == hasTip)
    return;
  hasTip_ = hasTip;
  [self setNeedsDisplay:YES];
}

@end
