// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/download/download_shelf_view.h"

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "grit/theme_resources.h"

@implementation DownloadShelfView

- (NSColor*)strokeColor {
  BOOL isKey = [[self window] isKeyWindow];
  ui::ThemeProvider* themeProvider = [[self window] themeProvider];
  return themeProvider ? themeProvider->GetNSColor(
      isKey ? ThemeService::COLOR_TOOLBAR_STROKE :
              ThemeService::COLOR_TOOLBAR_STROKE_INACTIVE, true) :
      [NSColor blackColor];
}

- (void)drawRect:(NSRect)rect {
  BOOL isKey = [[self window] isKeyWindow];
  ui::ThemeProvider* themeProvider = [[self window] themeProvider];
  if (!themeProvider)
    return;

  NSColor* backgroundImageColor =
      themeProvider->GetNSImageColorNamed(IDR_THEME_TOOLBAR, false);
  if (backgroundImageColor) {
    // We want our backgrounds for the shelf to be phased from the upper
    // left hand corner of the view.
    NSPoint phase = NSMakePoint(0, NSHeight([self bounds]));
    [[NSGraphicsContext currentContext] setPatternPhase:phase];
    [backgroundImageColor set];
    NSRectFill([self bounds]);
  } else {
    NSGradient* gradient = themeProvider->GetNSGradient(
        isKey ? ThemeService::GRADIENT_TOOLBAR :
                ThemeService::GRADIENT_TOOLBAR_INACTIVE);
    NSPoint startPoint = [self convertPoint:NSMakePoint(0, 0) fromView:nil];
    NSPoint endPoint =
        [self convertPoint:NSMakePoint(0, [self frame].size.height)
                  fromView:nil];

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

- (ViewID)viewID {
  return VIEW_ID_DOWNLOAD_SHELF;
}

@end
