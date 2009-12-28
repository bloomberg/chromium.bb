// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/infobar_gradient_view.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

const double kBackgroundColorTop[3] =
    {255.0 / 255.0, 242.0 / 255.0, 183.0 / 255.0};
const double kBackgroundColorBottom[3] =
    {250.0 / 255.0, 230.0 / 255.0, 145.0 / 255.0};

@implementation InfoBarGradientView
- (NSColor*)strokeColor {
  return [[self gtm_theme] strokeColorForStyle:GTMThemeStyleToolBar
                                         state:[[self window] isKeyWindow]];
}

- (void)drawRect:(NSRect)rect {
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
  NSGradient* gradient =
      [[[NSGradient alloc] initWithStartingColor:startingColor
                                     endingColor:endingColor] autorelease];
  [gradient drawInRect:[self bounds] angle:270];

  // Draw bottom stroke
  [[self strokeColor] set];
  NSRect borderRect, contentRect;
  NSDivideRect([self bounds], &borderRect, &contentRect, 1, NSMinYEdge);
  NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
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
