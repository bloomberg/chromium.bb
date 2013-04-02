// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_toolbar_view.h"

#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_properties.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_constants.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/nsview_additions.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "chrome/browser/ui/ntp_background_util.h"
#include "chrome/browser/ui/search/search_ui.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

const CGFloat kBorderRadius = 3.0;

@interface BookmarkBarToolbarView (Private)
- (void)drawAsDetachedBubble;
- (void)drawAsDetachedInstantExtendedUI;
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
    if (chrome::IsInstantExtendedAPIEnabled())
      [self drawAsDetachedInstantExtendedUI];
    else
      [self drawAsDetachedBubble];
  } else {
    NSPoint phase = [[self window] themePatternPhase];
    [[NSGraphicsContext currentContext] setPatternPhase:phase];
    [self drawBackgroundWithOpaque:YES];
  }
}

- (void)drawAsDetachedBubble {
  // The state of our morph; 1 is total bubble, 0 is the regular bar. We use it
  // to morph the bubble to a regular bar (shape and colour).
  CGFloat morph = [controller_ detachedMorphProgress];

  NSRect bounds = [self bounds];

  ui::ThemeProvider* themeProvider = [controller_ themeProvider];
  if (!themeProvider)
    return;

  gfx::ScopedNSGraphicsContextSaveGState scopedGState;

  // Draw the background.
  {
    // CanvasSkiaPaint draws to the NSGraphicsContext during its destructor, so
    // explicitly scope this.
    //
    // Paint the entire bookmark bar, even if the damage rect is much smaller
    // because PaintBackgroundDetachedMode() assumes that area's origin is
    // (0, 0) and that its size is the size of the bookmark bar.
    //
    // In practice, this sounds worse than it is because redraw time is still
    // minimal compared to the pause between frames of animations. We were
    // already repainting the rest of the bookmark bar below without setting a
    // clip area, anyway. Also, the only time we weren't asked to redraw the
    // whole bookmark bar is when the find bar is drawn over it.
    gfx::CanvasSkiaPaint canvas(bounds, true);
    gfx::Rect area(0, 0, NSWidth(bounds), NSHeight(bounds));

    NtpBackgroundUtil::PaintBackgroundDetachedMode(themeProvider, &canvas,
        area, [controller_ currentTabContentsHeight]);
  }

  // Draw our bookmark bar border on top of the background.
  NSRect frameRect =
      NSMakeRect(
          morph * bookmarks::kNTPBookmarkBarPadding,
          morph * bookmarks::kNTPBookmarkBarPadding,
          NSWidth(bounds) - 2 * morph * bookmarks::kNTPBookmarkBarPadding,
          NSHeight(bounds) - 2 * morph * bookmarks::kNTPBookmarkBarPadding);
  // Now draw a bezier path with rounded rectangles around the area.
  frameRect = NSInsetRect(frameRect, morph * 0.5, morph * 0.5);
  NSBezierPath* border =
      [NSBezierPath bezierPathWithRoundedRect:frameRect
                                      xRadius:(morph * kBorderRadius)
                                      yRadius:(morph * kBorderRadius)];

  // Draw the rounded rectangle.
  NSColor* toolbarColor =
      themeProvider->GetNSColor(ThemeProperties::COLOR_TOOLBAR, true);
  CGFloat alpha = morph * [toolbarColor alphaComponent];
  [[toolbarColor colorWithAlphaComponent:alpha] set];  // Set with opacity.
  [border fill];

  // Fade in/out the background.
  {
    gfx::ScopedNSGraphicsContextSaveGState bgScopedState;
    [border setClip];
    NSGraphicsContext* context = [NSGraphicsContext currentContext];
    CGContextRef cgContext = static_cast<CGContextRef>([context graphicsPort]);
    CGContextSetAlpha(cgContext, 1 - morph);
    CGContextBeginTransparencyLayer(cgContext, NULL);
    [context setPatternPhase:[[self window] themePatternPhase]];
    [self drawBackgroundWithOpaque:YES];
    CGContextEndTransparencyLayer(cgContext);
  }

  // Draw the border of the rounded rectangle.
  NSColor* borderColor = themeProvider->GetNSColor(
      ThemeProperties::COLOR_TOOLBAR_BUTTON_STROKE, true);
  alpha = morph * [borderColor alphaComponent];
  [[borderColor colorWithAlphaComponent:alpha] set];  // Set with opacity.
  [border stroke];

  // Fade in/out the divider.
  // TODO(viettrungluu): It's not obvious that this divider lines up exactly
  // with |BackgroundGradientView|'s (in fact, it probably doesn't).
  NSColor* strokeColor = [self strokeColor];
  alpha = (1 - morph) * [strokeColor alphaComponent];
  [[strokeColor colorWithAlphaComponent:alpha] set];
  NSBezierPath* divider = [NSBezierPath bezierPath];
  NSPoint dividerStart =
      NSMakePoint(morph * bookmarks::kNTPBookmarkBarPadding + morph * 0.5,
                  morph * bookmarks::kNTPBookmarkBarPadding + morph * 0.5);
  CGFloat dividerWidth =
      NSWidth(bounds) - 2 * morph * bookmarks::kNTPBookmarkBarPadding - 2 * 0.5;
  [divider moveToPoint:dividerStart];
  [divider relativeLineToPoint:NSMakePoint(dividerWidth, 0)];
  [divider stroke];
}

- (void)drawAsDetachedInstantExtendedUI {
  CGFloat morph = [controller_ detachedMorphProgress];
  NSRect bounds = [self bounds];
  ui::ThemeProvider* themeProvider = [controller_ themeProvider];
  if (!themeProvider)
    return;

  [[NSColor whiteColor] set];
  NSRectFill([self bounds]);

  // Overlay with a lighter background color.
  NSColor* toolbarColor = gfx::SkColorToCalibratedNSColor(
        chrome::GetDetachedBookmarkBarBackgroundColor(themeProvider));
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
    [context setPatternPhase:[[self window] themePatternPhase]];
    [self drawBackgroundWithOpaque:YES];
    CGContextEndTransparencyLayer(cgContext);
  }

  // Bottom stroke.
  NSColor* strokeColor = gfx::SkColorToCalibratedNSColor(
        chrome::GetDetachedBookmarkBarSeparatorColor(themeProvider));
  strokeColor = [[self strokeColor] blendedColorWithFraction:morph
                                                     ofColor:strokeColor];
  strokeColor = [strokeColor colorWithAlphaComponent:0.5];
  [strokeColor set];
  NSRect strokeRect = bounds;
  strokeRect.size.height = [self cr_lineWidth];
  NSRectFillUsingOperation(strokeRect, NSCompositeSourceOver);
}

@end  // @implementation BookmarkBarToolbarView
