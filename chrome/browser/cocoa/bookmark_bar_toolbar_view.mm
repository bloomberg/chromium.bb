// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_bar_toolbar_view.h"

#include "app/gfx/canvas_paint.h"
#include "app/theme_provider.h"
#include "base/gfx/rect.h"
#include "chrome/browser/browser_theme_provider.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/bookmark_bar_constants.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#include "chrome/browser/ntp_background_util.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

const CGFloat kBorderRadius = 3.0;

@interface BookmarkBarToolbarView (Private)
- (void)drawRectAsFloating:(NSRect)rect;
@end

@implementation BookmarkBarToolbarView

- (BOOL)isOpaque {
  return [controller_ drawAsFloatingBar];
}

- (void)drawRect:(NSRect)rect {
  if ([controller_ drawAsFloatingBar]) {
    [self drawRectAsFloating:rect];
  } else {
    NSPoint phase = [self gtm_themePatternPhase];
    [[NSGraphicsContext currentContext] setPatternPhase:phase];
    [self drawBackground];
  }
}

- (void)layoutViews {
  if ([controller_ drawAsFloatingBar]) {
    // The internal bookmark bar should have padding to center it.
    NSRect frame = [self frame];
    [buttonView_ setFrame:
                   NSMakeRect(bookmarks::kNTPBookmarkBarPadding,
                              bookmarks::kNTPBookmarkBarPadding,
                              NSWidth(frame) -
                              bookmarks::kNTPBookmarkBarPadding,
                              NSHeight(frame) -
                              bookmarks::kNTPBookmarkBarPadding)];
  } else {
    // The frame of our child should be equal to our frame.
    NSRect frame = [self frame];
    [buttonView_ setFrame:NSMakeRect(0, 0, NSWidth(frame), NSHeight(frame))];
  }
}

- (void)drawRectAsFloating:(NSRect)rect {
  NSRect bounds = [self bounds];

  ThemeProvider* themeProvider = [controller_ themeProvider];
  if (!themeProvider)
    return;

  NSGraphicsContext* theContext = [NSGraphicsContext currentContext];
  [theContext saveGraphicsState];

  // Draw the background
  {
    // CanvasPaint draws to the NSGraphicsContext during its destructor, so
    // explicitly scope this.
    gfx::CanvasPaint canvas(rect, true);
    gfx::Rect area(0, 0, NSWidth(rect), NSHeight(rect));

    NtpBackgroundUtil::PaintBackgroundDetachedMode(themeProvider, &canvas,
        area, [controller_ currentTabContentsHeight]);
  }

  // Draw our bookmark bar border on top of the background.
  NSRect frameRect =
      NSMakeRect(bookmarks::kNTPBookmarkBarPadding,
                 bookmarks::kNTPBookmarkBarPadding,
                 NSWidth(bounds) - 2 * bookmarks::kNTPBookmarkBarPadding,
                 NSHeight(bounds) - 2 * bookmarks::kNTPBookmarkBarPadding);
  // Now draw a bezier path with rounded rectangles around the area
  frameRect = NSInsetRect(frameRect, 0.5, 0.5);
  NSBezierPath* border =
      [NSBezierPath bezierPathWithRoundedRect:frameRect
                                      xRadius:kBorderRadius
                                      yRadius:kBorderRadius];
  NSColor* toolbarColor =
      [[self gtm_theme] backgroundColorForStyle:GTMThemeStyleToolBar
                                          state:GTMThemeStateActiveWindow];
  // workaround for default theme
  // TODO(alcor) next GTM update return nil for background color if not set;
  // http://crbug.com/25196
  if ([toolbarColor isEqual:[NSColor colorWithCalibratedWhite:0.5 alpha:1.0]])
    toolbarColor = nil;
  if (!toolbarColor)
    toolbarColor = [NSColor colorWithCalibratedWhite:0.9 alpha:1.0];
  [toolbarColor set];
  [border fill];

  NSColor* borderColor =
      [[self gtm_theme] strokeColorForStyle:GTMThemeStyleToolBarButton
                                      state:GTMThemeStateActiveWindow];
  [borderColor set];
  [border stroke];

  [theContext restoreGraphicsState];
}

@end  // @implementation BookmarkBarToolbarView
