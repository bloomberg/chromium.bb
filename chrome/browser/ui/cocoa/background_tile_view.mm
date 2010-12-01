// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/background_tile_view.h"

@implementation BackgroundTileView
@synthesize tileImage = tileImage_;

- (void)setTileImage:(NSImage*)tileImage {
  [tileImage_ autorelease];
  tileImage_ = [tileImage retain];
  [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)rect {
  // Tile within the view, so set the phase to start at the view bottom.
  NSPoint phase = NSMakePoint(0.0, NSMinY([self frame]));
  [[NSGraphicsContext currentContext] setPatternPhase:phase];

  if (tileImage_) {
    NSColor *color = [NSColor colorWithPatternImage:tileImage_];
    [color set];
  } else {
    // Something to catch the missing image
    [[NSColor magentaColor] set];
  }

  NSRectFill([self bounds]);
}

@end
