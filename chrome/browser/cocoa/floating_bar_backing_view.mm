// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/floating_bar_backing_view.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

@implementation FloatingBarBackingView

- (void)drawRect:(NSRect)rect {
  NSWindow* window = [self window];
  BOOL isMainWindow = [window isMainWindow];

  if (isMainWindow)
    [[NSColor windowFrameColor] set];
  else
    [[NSColor windowBackgroundColor] set];
  NSRectFill(rect);

  // Code basically clipped from browser_frame_view.mm, with modifications to
  // not use window rect and not draw rounded corners.
  NSRect bounds = [self bounds];

  // Draw our background color if we have one, otherwise fall back on
  // system drawing.
  GTMTheme* theme = [self gtm_theme];
  GTMThemeState state = isMainWindow ? GTMThemeStateActiveWindow
                                     : GTMThemeStateInactiveWindow;
  NSColor* color = [theme backgroundPatternColorForStyle:GTMThemeStyleWindow
                                                   state:state];
  if (color) {
    // If there is a theme pattern, draw it here.

    // To line up the background pattern with the patterns in the tabs the
    // background pattern in the window frame need to be moved up by two
    // pixels and left by 5.
    // This will make the themes look slightly different than in Windows/Linux
    // because of the differing heights between window top and tab top, but this
    // has been approved by UI.
    static const NSPoint kBrowserFrameViewPatternPhaseOffset = { -5, 2 };
    NSPoint phase = kBrowserFrameViewPatternPhaseOffset;
    phase.y += NSHeight(bounds);
    phase = [self gtm_themePatternPhase];
    [[NSGraphicsContext currentContext] setPatternPhase:phase];
    [color set];
    NSRectFill(rect);
  }

  // Check to see if we have an overlay image.
  NSImage* overlayImage = [theme valueForAttribute:@"overlay"
                                             style:GTMThemeStyleWindow
                                             state:state];
  if (overlayImage) {
    // Anchor to top-left and don't scale.
    NSSize overlaySize = [overlayImage size];
    NSRect imageFrame = NSMakeRect(0, 0, overlaySize.width, overlaySize.height);
    [overlayImage drawAtPoint:NSMakePoint(0, NSHeight(bounds) -
                                                 overlaySize.height)
                     fromRect:imageFrame
                    operation:NSCompositeSourceOver
                     fraction:1.0];
  }
}

@end  // @implementation FloatingBarBackingView
