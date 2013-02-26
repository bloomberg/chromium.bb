// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"

#import "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"
#include "skia/ext/skia_utils_mac.h"

@implementation ConstrainedWindowCustomWindow

- (id)initWithContentRect:(NSRect)contentRect {
  if ((self = [self initWithContentRect:contentRect
                              styleMask:NSBorderlessWindowMask
                                backing:NSBackingStoreBuffered
                                  defer:NO])) {
    scoped_nsobject<NSView> contentView(
        [[ConstrainedWindowCustomWindowContentView alloc]
            initWithFrame:NSZeroRect]);
    [self setContentView:contentView];
  }
  return self;
}

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)windowStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)deferCreation {
  if ((self = [super initWithContentRect:contentRect
                               styleMask:NSBorderlessWindowMask
                                 backing:bufferingType
                                   defer:NO])) {
    [self setHasShadow:YES];
    [self setBackgroundColor:[NSColor clearColor]];
    [self setOpaque:NO];
    [self setReleasedWhenClosed:NO];
  }
  return self;
}

- (BOOL)canBecomeKeyWindow {
  return YES;
}

- (NSRect)frameRectForContentRect:(NSRect)windowContent {
  id<ConstrainedWindowSheet> sheet = [ConstrainedWindowSheetController
      sheetForOverlayWindow:[self parentWindow]];
  ConstrainedWindowSheetController* sheetController =
      [ConstrainedWindowSheetController controllerForSheet:sheet];

  // Sheet controller may be nil if this window hasn't been shown yet.
  if (!sheetController)
    return windowContent;

  NSRect frame;
  frame.origin = [sheetController originForSheet:sheet
                                  withWindowSize:windowContent.size];
  frame.size = windowContent.size;
  return frame;
}

@end

@implementation ConstrainedWindowCustomWindowContentView

- (void)drawRect:(NSRect)rect {
  NSBezierPath* path = [NSBezierPath
      bezierPathWithRoundedRect:[self bounds]
                        xRadius:chrome_style::kBorderRadius
                        yRadius:chrome_style::kBorderRadius];
  [gfx::SkColorToCalibratedNSColor(
      chrome_style::GetBackgroundColor()) set];
  [path fill];

  [[self window] invalidateShadow];
}

@end
