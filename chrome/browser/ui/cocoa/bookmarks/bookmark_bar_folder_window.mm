// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_folder_window.h"

#import "base/logging.h"
#import "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_constants.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_folder_controller.h"
#import "third_party/GTM/AppKit/GTMNSColor+Luminance.h"
#import "third_party/GTM/AppKit/GTMNSBezierPath+RoundRect.h"

using bookmarks::kBookmarkBarMenuCornerRadius;

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

- (BOOL)canBecomeKeyWindow {
  return YES;
}

- (BOOL)canBecomeMainWindow {
  return NO;
}

// Override of keyDown as the NSWindow default implementation beeps.
- (void)keyDown:(NSEvent *)theEvent {
}

@end


@implementation BookmarkBarFolderWindowContentView

- (void)drawRect:(NSRect)rect {
  // Like NSMenus, only the bottom corners are rounded.
  NSBezierPath* bezier =
      [NSBezierPath gtm_bezierPathWithRoundRect:[self bounds]
                            topLeftCornerRadius:kBookmarkBarMenuCornerRadius
                           topRightCornerRadius:kBookmarkBarMenuCornerRadius
                         bottomLeftCornerRadius:kBookmarkBarMenuCornerRadius
                        bottomRightCornerRadius:kBookmarkBarMenuCornerRadius];
  NSColor* startColor = [NSColor colorWithCalibratedWhite:0.91 alpha:1.0];
  NSColor* midColor =
      [startColor gtm_colorAdjustedFor:GTMColorationLightMidtone faded:YES];
  NSColor* endColor =
      [startColor gtm_colorAdjustedFor:GTMColorationLightPenumbra faded:YES];

  scoped_nsobject<NSGradient> gradient(
    [[NSGradient alloc] initWithColorsAndLocations:startColor, 0.0,
                        midColor, 0.25,
                        endColor, 0.5,
                        midColor, 0.75,
                        startColor, 1.0,
                        nil]);
  [gradient drawInBezierPath:bezier angle:0.0];
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
