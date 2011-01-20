// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/infobars/infobar_gradient_view.h"

#include "base/scoped_nsobject.h"
#import "chrome/browser/themes/browser_theme_provider.h"
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
      active ? BrowserThemeProvider::COLOR_TOOLBAR_STROKE :
               BrowserThemeProvider::COLOR_TOOLBAR_STROKE_INACTIVE,
      true);
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
