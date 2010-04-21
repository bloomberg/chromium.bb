// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_bar_folder_window.h"

#import "base/scoped_nsobject.h"
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
}

@end
