// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/background_gradient_view.h"

#import "chrome/browser/themes/theme_properties.h"
#import "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/nsview_additions.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "grit/theme_resources.h"

@interface BackgroundGradientView (Private)
- (void)commonInit;
- (NSColor*)backgroundImageColor;
@end

@implementation BackgroundGradientView

@synthesize showsDivider = showsDivider_;

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    [self commonInit];
  }
  return self;
}

- (id)initWithCoder:(NSCoder*)decoder {
  if ((self = [super initWithCoder:decoder])) {
    [self commonInit];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)commonInit {
  showsDivider_ = YES;
}

- (void)setShowsDivider:(BOOL)show {
  if (showsDivider_ == show)
    return;
  showsDivider_ = show;
  [self setNeedsDisplay:YES];
}

- (void)drawBackgroundWithOpaque:(BOOL)opaque {
  const NSRect bounds = [self bounds];

  if (opaque) {
    // If the background image is semi transparent then we need something
    // to blend against. Using 20% black gives us a color similar to Windows.
    [[NSColor colorWithCalibratedWhite:0.2 alpha:1.0] set];
    NSRectFill(bounds);
  }

  [[self backgroundImageColor] set];
  NSRectFillUsingOperation(bounds, NSCompositeSourceOver);

  if (showsDivider_) {
    // Draw bottom stroke
    [[self strokeColor] set];
    NSRect borderRect, contentRect;
    NSDivideRect(bounds, &borderRect, &contentRect, [self cr_lineWidth],
                 NSMinYEdge);
    NSRectFillUsingOperation(borderRect, NSCompositeSourceOver);
  }
}

- (NSColor*)strokeColor {
  NSWindow* window = [self window];

  // Some views have a child NSWindow between them and the window that is
  // active (e.g, OmniboxPopupTopSeparatorView). For these, check the status
  // of parentWindow instead. Note that this is not tracked correctly (but
  // the views that do this appear to be removed when the window loses focus
  // anyway).
  if ([window parentWindow])
    window = [window parentWindow];
  BOOL isActive = [window isMainWindow];

  ui::ThemeProvider* themeProvider = [window themeProvider];
  if (!themeProvider)
    return [NSColor blackColor];
  return themeProvider->GetNSColor(
      isActive ? ThemeProperties::COLOR_TOOLBAR_STROKE :
                 ThemeProperties::COLOR_TOOLBAR_STROKE_INACTIVE);
}

- (NSColor*)backgroundImageColor {
  ThemeService* themeProvider =
      static_cast<ThemeService*>([[self window] themeProvider]);
  if (!themeProvider)
    return [[self window] backgroundColor];

  // Themes don't have an inactive image so only look for one if there's no
  // theme.
  BOOL isActive = [[self window] isMainWindow];
  if (!isActive && themeProvider->UsingDefaultTheme()) {
    NSColor* color = themeProvider->GetNSImageColorNamed(
        IDR_THEME_TOOLBAR_INACTIVE);
    if (color)
      return color;
  }

  return themeProvider->GetNSImageColorNamed(IDR_THEME_TOOLBAR);
}

- (void)windowFocusDidChange:(NSNotification*)notification {
  // Some child views will indirectly use BackgroundGradientView by calling an
  // ancestor's draw function (e.g, BookmarkButtonView). Call setNeedsDisplay
  // on all descendants to ensure that these views re-draw.
  // TODO(ccameron): Enable these views to listen for focus notifications
  // directly.
  [self cr_recursivelySetNeedsDisplay:YES];
}

- (void)viewWillMoveToWindow:(NSWindow*)window {
  if ([self window]) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:self
                  name:NSWindowDidBecomeKeyNotification
                object:[self window]];
    [[NSNotificationCenter defaultCenter]
        removeObserver:self
                  name:NSWindowDidBecomeMainNotification
                object:[self window]];
  }
  if (window) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(windowFocusDidChange:)
               name:NSWindowDidBecomeMainNotification
             object:window];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(windowFocusDidChange:)
               name:NSWindowDidResignMainNotification
             object:window];
    // The new window for the view may have a different focus state than the
    // last window this view was part of. Force a re-draw to ensure that the
    // view draws the right state.
    [self windowFocusDidChange:nil];
  }
  [super viewWillMoveToWindow:window];
}

- (void)setFrameOrigin:(NSPoint)origin {
  // The background color depends on the view's vertical position. This impacts
  // any child views that draw using this view's functions.
  if (NSMinY([self frame]) != origin.y)
    [self cr_recursivelySetNeedsDisplay:YES];

  [super setFrameOrigin:origin];
}

@end
