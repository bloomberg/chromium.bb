// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/background_gradient_view.h"

#import "chrome/browser/themes/theme_properties.h"
#import "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "grit/theme_resources.h"
#import "ui/base/cocoa/nsgraphics_context_additions.h"
#import "ui/base/cocoa/nsview_additions.h"

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

- (void)commonInit {
  showsDivider_ = YES;
}

- (void)setShowsDivider:(BOOL)show {
  if (showsDivider_ == show)
    return;
  showsDivider_ = show;
  [self setNeedsDisplay:YES];
}

- (NSPoint)patternPhase {
  return [[self window]
      themeImagePositionForAlignment:THEME_IMAGE_ALIGN_WITH_TAB_STRIP];
}

- (void)drawBackground:(NSRect)dirtyRect {
  [[NSGraphicsContext currentContext]
      cr_setPatternPhase:[self patternPhase]
                 forView:[self cr_viewBeingDrawnTo]];

  ui::ThemeProvider* themeProvider = [[self window] themeProvider];
  if (themeProvider && !themeProvider->UsingSystemTheme()) {
    // If the background image is semi transparent then we need something
    // to blend against. Using 20% black gives us a color similar to Windows.
    [[NSColor colorWithCalibratedWhite:0.2 alpha:1.0] set];
    NSRectFill(dirtyRect);
  }

  [[self backgroundImageColor] set];
  NSRectFillUsingOperation(dirtyRect, NSCompositeSourceOver);

  if (showsDivider_) {
    // Draw bottom stroke
    NSRect borderRect, contentRect;
    NSDivideRect([self bounds], &borderRect, &contentRect, [self cr_lineWidth],
                 NSMinYEdge);
    if (NSIntersectsRect(borderRect, dirtyRect)) {
      [[self strokeColor] set];
      NSRectFillUsingOperation(NSIntersectionRect(borderRect, dirtyRect),
                               NSCompositeSourceOver);
    }
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
  ui::ThemeProvider* themeProvider = [[self window] themeProvider];
  if (!themeProvider)
    return [[self window] backgroundColor];

  // Themes don't have an inactive image so only look for one if there's no
  // theme.
  BOOL isActive = [[self window] isMainWindow];
  if (!isActive && themeProvider->UsingSystemTheme()) {
    NSColor* color = themeProvider->GetNSImageColorNamed(
        IDR_THEME_TOOLBAR_INACTIVE);
    if (color)
      return color;
  }

  return themeProvider->GetNSImageColorNamed(IDR_THEME_TOOLBAR);
}

- (void)viewDidMoveToWindow {
  [super viewDidMoveToWindow];
  if ([self window]) {
    // The new window for the view may have a different focus state than the
    // last window this view was part of.
    // This happens when the view is moved into a TabWindowOverlayWindow for
    // tab dragging.
    [self windowDidChangeActive];
  }
}

- (void)viewWillStartLiveResize {
  [super viewWillStartLiveResize];

  ui::ThemeProvider* themeProvider = [[self window] themeProvider];
  if (themeProvider && themeProvider->UsingSystemTheme()) {
    // The default theme's background image is a subtle texture pattern that
    // we can scale without being easily noticed. Optimize this case by
    // skipping redraws during live resize.
    [self setLayerContentsRedrawPolicy:
        NSViewLayerContentsRedrawOnSetNeedsDisplay];
  }
}

- (void)viewDidEndLiveResize {
  [super viewDidEndLiveResize];

  if ([self layerContentsRedrawPolicy] !=
      NSViewLayerContentsRedrawDuringViewResize) {
    // If we have been scaling the layer during live resize, now is the time to
    // redraw the layer.
    [self setLayerContentsRedrawPolicy:
        NSViewLayerContentsRedrawDuringViewResize];
    [self setNeedsDisplay:YES];
  }
}

- (void)setFrameOrigin:(NSPoint)origin {
  // The background color depends on the view's vertical position. This impacts
  // any child views that draw using this view's functions.
  // When resizing the window, the view's vertical position (NSMinY) may change
  // even though our relative position to the nearest window edge is still the
  // same. Don't redraw unnecessarily in this case.
  if (![self inLiveResize] && NSMinY([self frame]) != origin.y)
    [self cr_recursivelySetNeedsDisplay:YES];

  [super setFrameOrigin:origin];
}

// ThemedWindowDrawing implementation.

- (void)windowDidChangeTheme {
  [self setNeedsDisplay:YES];
}

- (void)windowDidChangeActive {
  [self setNeedsDisplay:YES];
}

@end
