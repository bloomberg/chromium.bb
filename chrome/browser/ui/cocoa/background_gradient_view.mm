// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/background_gradient_view.h"

#import "chrome/browser/themes/browser_theme_provider.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "grit/theme_resources.h"

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

- (void)drawBackground {
  BOOL isKey = [[self window] isKeyWindow];
  ui::ThemeProvider* themeProvider = [[self window] themeProvider];
  if (themeProvider) {
    NSColor* backgroundImageColor =
        themeProvider->GetNSImageColorNamed(IDR_THEME_TOOLBAR, false);
    if (backgroundImageColor) {
      [backgroundImageColor set];
      NSRectFill([self bounds]);
    } else {
      CGFloat winHeight = NSHeight([[self window] frame]);
      NSGradient* gradient = themeProvider->GetNSGradient(
          isKey ? BrowserThemeProvider::GRADIENT_TOOLBAR :
                  BrowserThemeProvider::GRADIENT_TOOLBAR_INACTIVE);
      NSPoint startPoint =
          [self convertPoint:NSMakePoint(0, winHeight - kToolbarTopOffset)
                    fromView:nil];
      NSPoint endPoint =
          NSMakePoint(0, winHeight - kToolbarTopOffset - kToolbarMaxHeight);
      endPoint = [self convertPoint:endPoint fromView:nil];

      [gradient drawFromPoint:startPoint
                      toPoint:endPoint
                      options:(NSGradientDrawsBeforeStartingLocation |
                               NSGradientDrawsAfterEndingLocation)];
    }

    if (showsDivider_) {
      // Draw bottom stroke
      [[self strokeColor] set];
      NSRect borderRect, contentRect;
      NSDivideRect([self bounds], &borderRect, &contentRect, 1, NSMinYEdge);
      NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
    }
  }
}

- (NSColor*)strokeColor {
  BOOL isKey = [[self window] isKeyWindow];
  ui::ThemeProvider* themeProvider = [[self window] themeProvider];
  if (!themeProvider)
    return [NSColor blackColor];
  return themeProvider->GetNSColor(
      isKey ? BrowserThemeProvider::COLOR_TOOLBAR_STROKE :
              BrowserThemeProvider::COLOR_TOOLBAR_STROKE_INACTIVE, true);
}

@end
