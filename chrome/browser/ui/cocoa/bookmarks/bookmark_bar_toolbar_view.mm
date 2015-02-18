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
#include "chrome/browser/ui/search/search_ui.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

@interface BookmarkBarToolbarView (Private)
- (void)drawAsDetachedBubble:(NSRect)dirtyRect;
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

- (void)drawRect:(NSRect)dirtyRect {
  if ([controller_ isInState:BookmarkBar::DETACHED] ||
      [controller_ isAnimatingToState:BookmarkBar::DETACHED] ||
      [controller_ isAnimatingFromState:BookmarkBar::DETACHED]) {
    [self drawAsDetachedBubble:dirtyRect];
  } else {
    [self drawBackground:dirtyRect];
  }
}

- (void)drawAsDetachedBubble:(NSRect)dirtyRect {
  CGFloat morph = [controller_ detachedMorphProgress];
  ThemeService* themeService = [controller_ themeService];
  if (!themeService)
    return;

  [[NSColor whiteColor] set];
  NSRectFill(dirtyRect);

  // Overlay with a lighter background color.
  NSColor* toolbarColor = gfx::SkColorToCalibratedNSColor(
      chrome::GetDetachedBookmarkBarBackgroundColor(themeService));
  CGFloat alpha = morph * [toolbarColor alphaComponent];
  [[toolbarColor colorWithAlphaComponent:alpha] set];
  NSRectFillUsingOperation(dirtyRect, NSCompositeSourceOver);

  // Fade in/out the background.
  {
    gfx::ScopedNSGraphicsContextSaveGState bgScopedState;
    NSGraphicsContext* context = [NSGraphicsContext currentContext];
    CGContextRef cgContext = static_cast<CGContextRef>([context graphicsPort]);
    CGContextSetAlpha(cgContext, 1 - morph);
    CGContextBeginTransparencyLayer(cgContext, NULL);
    [self drawBackground:dirtyRect];
    CGContextEndTransparencyLayer(cgContext);
  }

  // Bottom stroke.
  NSRect strokeRect = [self bounds];
  strokeRect.size.height = [self cr_lineWidth];
  if (NSIntersectsRect(strokeRect, dirtyRect)) {
    NSColor* strokeColor = gfx::SkColorToCalibratedNSColor(
        chrome::GetDetachedBookmarkBarSeparatorColor(themeService));
    strokeColor = [[self strokeColor] blendedColorWithFraction:morph
                                                       ofColor:strokeColor];
    strokeColor = [strokeColor colorWithAlphaComponent:0.5];
    [strokeColor set];
    NSRectFillUsingOperation(NSIntersectionRect(strokeRect, dirtyRect),
                             NSCompositeSourceOver);
  }
}

@end  // @implementation BookmarkBarToolbarView
