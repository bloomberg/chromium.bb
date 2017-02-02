// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/download/download_shelf_view_cocoa.h"

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/cocoa/hover_close_button.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/material_design/material_design_controller.h"

@implementation DownloadShelfView

// For programmatic instantiations in unit tests.
- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    self.dividerEdge = NSMaxYEdge;
  }
  return self;
}

// For nib instantiations in production.
- (id)initWithCoder:(NSCoder*)decoder {
  if ((self = [super initWithCoder:decoder])) {
    self.dividerEdge = NSMaxYEdge;
  }
  return self;
}

- (NSPoint)patternPhase {
  // We want our backgrounds for the shelf to be phased from the upper
  // left hand corner of the view. Offset it by tab height so that the
  // background matches the toolbar background.
  return NSMakePoint(
      0, NSHeight([self bounds]) + [TabStripController defaultTabHeight]);
}

- (void)drawRect:(NSRect)dirtyRect {
  [self drawBackground:dirtyRect];

  // Draw the top highlight
  NSRect borderRect, contentRect;
  NSDivideRect([self bounds], &borderRect, &contentRect, [self cr_lineWidth],
               NSMaxYEdge);
  borderRect.origin.y -= [self cr_lineWidth];
  if (NSIntersectsRect(borderRect, dirtyRect)) {
    const ui::ThemeProvider* themeProvider = [[self window] themeProvider];
    if (themeProvider) {
      int resourceName = themeProvider->UsingSystemTheme()
                             ? ThemeProperties::COLOR_TOOLBAR_BEZEL
                             : ThemeProperties::COLOR_TOOLBAR;
      NSColor* highlightColor = themeProvider->GetNSColor(resourceName);
      if (highlightColor) {
        [highlightColor set];
        NSRectFillUsingOperation(NSIntersectionRect(borderRect, dirtyRect),
                                 NSCompositeSourceOver);
      }
    }
  }
}

- (void)viewWillDraw {
  if (const ui::ThemeProvider* themeProvider = [[self window] themeProvider]) {
    [closeButton_
        setIconColor:themeProvider->GetColor(ThemeProperties::COLOR_TAB_TEXT)];
  }
  [super viewWillDraw];
}

// Mouse down events on the download shelf should not allow dragging the parent
// window around.
- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

- (ViewID)viewID {
  return VIEW_ID_DOWNLOAD_SHELF;
}

- (BOOL)isOpaque {
  return YES;
}

@end
