// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/floating_bar_backing_view.h"
#import "chrome/browser/cocoa/GTMTheme.h"

@implementation FloatingBarBackingView

- (void)drawRect:(NSRect)rect {
  NSWindow* window = [self window];
  BOOL isMainWindow = [window isMainWindow];

  if (isMainWindow)
    [[NSColor windowFrameColor] set];
  else
    [[NSColor windowBackgroundColor] set];
  NSRectFill(rect);

  // The code here mirrors the code in browser_frame_view.mm, with modifications
  // to not use window rect and not draw rounded corners.
  NSRect bounds = [self bounds];

  // Draw our background image or theme pattern, if one exists.
  GTMTheme* theme = [self gtm_theme];
  GTMThemeState state = isMainWindow ? GTMThemeStateActiveWindow
                                     : GTMThemeStateInactiveWindow;
  NSColor* color = [theme backgroundPatternColorForStyle:GTMThemeStyleWindow
                                                   state:state];
  if (color) {
    // The titlebar/tabstrip header on the mac is slightly smaller than on
    // Windows.  To keep the window background lined up with the tab and toolbar
    // patterns, we have to shift the pattern slightly, rather than simply
    // drawing it from the top left corner.  The offset below was empirically
    // determined in order to line these patterns up.
    //
    // This will make the themes look slightly different than in Windows/Linux
    // because of the differing heights between window top and tab top, but this
    // has been approved by UI.
    static const NSPoint kBrowserFrameViewPatternPhaseOffset = { -5, 2 };
    NSPoint offsetTopLeft = kBrowserFrameViewPatternPhaseOffset;
    offsetTopLeft.y += NSHeight(bounds);

    // We want the pattern to start at the upper left corner of the floating
    // bar, but the phase needs to be in the coordinate system of
    // |windowChromeView|.
    NSView* windowChromeView = [[[self window] contentView] superview];
    NSPoint phase = [self convertPoint:offsetTopLeft toView:windowChromeView];

    // Apply the offset.
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

// Eat all mouse events (and do *not* pass them on to the next responder!).
- (void)mouseDown:(NSEvent*)event {}
- (void)rightMouseDown:(NSEvent*)event {}
- (void)otherMouseDown:(NSEvent*)event {}
- (void)rightMouseUp:(NSEvent*)event {}
- (void)otherMouseUp:(NSEvent*)event {}
- (void)mouseMoved:(NSEvent*)event {}
- (void)mouseDragged:(NSEvent*)event {}
- (void)rightMouseDragged:(NSEvent*)event {}
- (void)otherMouseDragged:(NSEvent*)event {}

// Eat this too, except that ...
- (void)mouseUp:(NSEvent*)event {
  // a double-click in the blank area should minimize.
  if ([event clickCount] == 2)
    [[self window] performMiniaturize:self];
}

@end  // @implementation FloatingBarBackingView
