// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmarks/bookmark_bar_folder_window.h"

#import "base/logging.h"
#include "base/nsimage_cache_mac.h"
#import "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmarks/bookmark_bar_folder_controller.h"
#import "chrome/browser/cocoa/image_utils.h"
#import "third_party/GTM/AppKit/GTMNSColor+Luminance.h"
#import "third_party/GTM/AppKit/GTMNSBezierPath+RoundRect.h"


@implementation BookmarkBarFolderWindow

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)windowStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)deferCreation {
  if ((self = [super initWithContentRect:contentRect
                               styleMask:NSBorderlessWindowMask // override
                                 backing:bufferingType
                                   defer:deferCreation])) {
    [self setBackgroundColor:[NSColor clearColor]];
    [self setOpaque:NO];
  }
  return self;
}

@end


namespace {
// Corner radius for our bookmark bar folder window.
// Copied from bubble_view.mm.
const CGFloat kViewCornerRadius = 4.0;
}

@implementation BookmarkBarFolderWindowContentView

- (void)awakeFromNib {
  arrowUpImage_.reset([nsimage_cache::ImageNamed(@"menu_overflow_up.pdf")
                                    retain]);
  arrowDownImage_.reset([nsimage_cache::ImageNamed(@"menu_overflow_down.pdf")
                                      retain]);
}

// Draw the arrows at the top and bottom of the folder window as a
// visual indication that scrolling is possible.  We always draw the
// scrolling arrows; when not relevant (e.g. when not scrollable), the
// scroll view overlaps the window and the arrows aren't visible.
- (void)drawScrollArrows:(NSRect)rect {
  NSRect visibleRect = [self bounds];

  // On top
  NSRect imageRect = NSZeroRect;
  imageRect.size = [arrowUpImage_ size];
  NSRect drawRect = NSOffsetRect(
      imageRect,
      (NSWidth(visibleRect) - NSWidth(imageRect)) / 2,
      NSHeight(visibleRect) - NSHeight(imageRect));
  [arrowUpImage_ drawInRect:drawRect
                   fromRect:imageRect
                  operation:NSCompositeSourceOver
                   fraction:1.0
               neverFlipped:YES];

  // On bottom
  imageRect = NSZeroRect;
  imageRect.size = [arrowDownImage_ size];
  drawRect = NSOffsetRect(imageRect,
                          (NSWidth(visibleRect) - NSWidth(imageRect)) / 2,
                          0);
  [arrowDownImage_ drawInRect:drawRect
                     fromRect:imageRect
                    operation:NSCompositeSourceOver
                     fraction:1.0
                 neverFlipped:YES];
}

- (void)drawRect:(NSRect)rect {
  NSRect bounds = [self bounds];
  // Like NSMenus, only the bottom corners are rounded.
  NSBezierPath* bezier =
      [NSBezierPath gtm_bezierPathWithRoundRect:bounds
                            topLeftCornerRadius:0
                           topRightCornerRadius:0
                         bottomLeftCornerRadius:kViewCornerRadius
                        bottomRightCornerRadius:kViewCornerRadius];
  [bezier closePath];

  // TODO(jrg): share code with info_bubble_view.mm?  Or bubble_view.mm?
  NSColor* base_color = [NSColor colorWithCalibratedWhite:0.5 alpha:1.0];
  NSColor* startColor =
      [base_color gtm_colorAdjustedFor:GTMColorationLightHighlight
                                 faded:YES];
  NSColor* midColor =
      [base_color gtm_colorAdjustedFor:GTMColorationLightMidtone
                                 faded:YES];
  NSColor* endColor =
      [base_color gtm_colorAdjustedFor:GTMColorationLightShadow
                                 faded:YES];
  NSColor* glowColor =
      [base_color gtm_colorAdjustedFor:GTMColorationLightPenumbra
                                 faded:YES];

  scoped_nsobject<NSGradient> gradient(
    [[NSGradient alloc] initWithColorsAndLocations:startColor, 0.0,
                        midColor, 0.25,
                        endColor, 0.5,
                        glowColor, 0.75,
                        nil]);
  [gradient drawInBezierPath:bezier angle:0.0];

  [self drawScrollArrows:rect];
}

@end


@implementation BookmarkBarFolderWindowScrollView

// We want "draw background" of the NSScrollView in the xib to be NOT
// checked.  That allows us to round the bottom corners of the folder
// window.  However that also allows some scrollWheel: events to leak
// into the NSWindow behind it (even in a different application).
// Better to plug the scroll leak than to round corners for M5.
- (void)scrollWheel:(NSEvent *)theEvent {
  DCHECK([[[self window] windowController]
           respondsToSelector:@selector(scrollWheel:)]);
  [[[self window] windowController] scrollWheel:theEvent];
}

@end
