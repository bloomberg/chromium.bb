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
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(windowFocusDidChange:)
             name:NSApplicationWillBecomeActiveNotification
           object:NSApp];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(windowFocusDidChange:)
             name:NSApplicationWillResignActiveNotification
           object:NSApp];
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
  if (![[self window] isMainWindow] && themeProvider->UsingDefaultTheme()) {
    NSColor* color = themeProvider->GetNSImageColorNamed(
        IDR_THEME_TOOLBAR_INACTIVE);
    if (color)
      return color;
  }

  return themeProvider->GetNSImageColorNamed(IDR_THEME_TOOLBAR);
}

- (void)windowFocusDidChange:(NSNotification*)notification {
  // The background color depends on the window's focus state.
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
               name:NSWindowDidBecomeKeyNotification
             object:[self window]];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(windowFocusDidChange:)
               name:NSWindowDidBecomeMainNotification
             object:[self window]];
  }
  [super viewWillMoveToWindow:window];
}

- (void)setFrameOrigin:(NSPoint)origin {
  // The background color depends on the view's vertical position.
  if (NSMinY([self frame]) != origin.y)
    [self setNeedsDisplay:YES];

  [super setFrameOrigin:origin];
}

@end
