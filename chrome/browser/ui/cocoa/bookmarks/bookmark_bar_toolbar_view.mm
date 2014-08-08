// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_toolbar_view.h"

#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_constants.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/nsview_additions.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "chrome/browser/ui/search/search_ui.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/nsgraphics_context_additions.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

@interface BookmarkBarToolbarView (Private)
- (void)drawAsDetachedBubble;
@end

@implementation BookmarkBarToolbarView

- (BOOL)isOpaque {
  return [controller_ isInState:BookmarkBar::DETACHED];
}

- (void)resetCursorRects {
  NSCursor *arrow = [NSCursor arrowCursor];
  [self addCursorRect:[self visibleRect] cursor:arrow];
  [arrow setOnMouseEntered:YES];
}

- (void)drawRect:(NSRect)rect {
  if ([controller_ isInState:BookmarkBar::DETACHED] ||
      [controller_ isAnimatingToState:BookmarkBar::DETACHED] ||
      [controller_ isAnimatingFromState:BookmarkBar::DETACHED]) {
    [self drawAsDetachedBubble];
  } else {
    NSPoint position = [[self window]
        themeImagePositionForAlignment:THEME_IMAGE_ALIGN_WITH_TAB_STRIP];
    [[NSGraphicsContext currentContext]
        cr_setPatternPhase:position
                   forView:[self cr_viewBeingDrawnTo]];
    [self drawBackgroundWithOpaque:YES];
  }
}

- (void)drawAsDetachedBubble {
  CGFloat morph = [controller_ detachedMorphProgress];
  NSRect bounds = [self bounds];
  ThemeService* themeService = [controller_ themeService];
  if (!themeService)
    return;

  [[NSColor whiteColor] set];
  NSRectFill([self bounds]);

  // Overlay with a lighter background color.
  NSColor* toolbarColor = gfx::SkColorToCalibratedNSColor(
        chrome::GetDetachedBookmarkBarBackgroundColor(themeService));
  CGFloat alpha = morph * [toolbarColor alphaComponent];
  [[toolbarColor colorWithAlphaComponent:alpha] set];
  NSRectFillUsingOperation(bounds, NSCompositeSourceOver);

  // Fade in/out the background.
  {
    gfx::ScopedNSGraphicsContextSaveGState bgScopedState;
    NSGraphicsContext* context = [NSGraphicsContext currentContext];
    CGContextRef cgContext = static_cast<CGContextRef>([context graphicsPort]);
    CGContextSetAlpha(cgContext, 1 - morph);
    CGContextBeginTransparencyLayer(cgContext, NULL);
    NSPoint position = [[self window]
        themeImagePositionForAlignment:THEME_IMAGE_ALIGN_WITH_TAB_STRIP];
    [context cr_setPatternPhase:position forView:[self cr_viewBeingDrawnTo]];
    [self drawBackgroundWithOpaque:YES];
    CGContextEndTransparencyLayer(cgContext);
  }

  // Bottom stroke.
  NSColor* strokeColor = gfx::SkColorToCalibratedNSColor(
        chrome::GetDetachedBookmarkBarSeparatorColor(themeService));
  strokeColor = [[self strokeColor] blendedColorWithFraction:morph
                                                     ofColor:strokeColor];
  strokeColor = [strokeColor colorWithAlphaComponent:0.5];
  [strokeColor set];
  NSRect strokeRect = bounds;
  strokeRect.size.height = [self cr_lineWidth];
  NSRectFillUsingOperation(strokeRect, NSCompositeSourceOver);
}

@end  // @implementation BookmarkBarToolbarView
