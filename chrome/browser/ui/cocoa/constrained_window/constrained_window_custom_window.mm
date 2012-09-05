// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"

#import "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/constrained_window.h"
#include "skia/ext/skia_utils_mac.h"

// The content view for the custom window.
@interface ConstrainedWindowCustomWindowContentView : NSView
@end

@implementation ConstrainedWindowCustomWindow

- (id)initWithContentRect:(NSRect)contentRect {
  if ((self = [super initWithContentRect:contentRect
                               styleMask:NSBorderlessWindowMask
                                 backing:NSBackingStoreBuffered
                                   defer:NO])) {
    [self setHasShadow:YES];
    [self setBackgroundColor:[NSColor clearColor]];
    [self setOpaque:NO];
    scoped_nsobject<NSView> contentView(
        [[ConstrainedWindowCustomWindowContentView alloc]
            initWithFrame:NSZeroRect]);
    [self setContentView:contentView];
  }
  return self;
}

- (BOOL)canBecomeKeyWindow {
  return YES;
}

@end

@implementation ConstrainedWindowCustomWindowContentView

- (void)drawRect:(NSRect)rect {
  NSBezierPath* path = [NSBezierPath
      bezierPathWithRoundedRect:[self bounds]
                        xRadius:ConstrainedWindow::kBorderRadius
                        yRadius:ConstrainedWindow::kBorderRadius];
  [gfx::SkColorToCalibratedNSColor(
      ConstrainedWindow::GetBackgroundColor()) set];
  [path fill];

  [[self window] invalidateShadow];
}

@end
