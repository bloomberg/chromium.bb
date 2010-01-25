// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/fullscreen_window.h"

#include "base/mac_util.h"

@implementation FullscreenWindow

// Make sure our designated initializer gets called.
- (id)init {
  return [self initForScreen:[NSScreen mainScreen]];
}

- (id)initForScreen:(NSScreen*)screen {
  NSRect contentRect;
  contentRect.origin = NSZeroPoint;
  contentRect.size = [screen frame].size;

  if ((self = [super initWithContentRect:contentRect
                               styleMask:NSBorderlessWindowMask
                                 backing:NSBackingStoreBuffered
                                   defer:YES
                                  screen:screen])) {
    [self setReleasedWhenClosed:NO];
  }
  return self;
}

// According to
// http://www.cocoabuilder.com/archive/message/cocoa/2006/6/19/165953,
// NSBorderlessWindowMask windows cannot become key or main.
// In our case, however, we don't want that behavior, so we override
// canBecomeKeyWindow and canBecomeMainWindow.

- (BOOL)canBecomeKeyWindow {
  return YES;
}

- (BOOL)canBecomeMainWindow {
  return YES;
}

// We need our own version, since the default one wants to flash the close
// button (and possibly other things), which results in nothing happening.
- (void)performClose:(id)sender {
  BOOL shouldClose = YES;

  // If applicable, check if this window should close.
  id delegate = [self delegate];
  if ([delegate respondsToSelector:@selector(windowShouldClose:)])
    shouldClose = [delegate windowShouldClose:self];

  if (shouldClose) {
    [self close];
    mac_util::ReleaseFullScreen();
  }
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  // Explicitly enable |-performClose:| (see above); otherwise the fact that
  // this window does not have a close button results in it being disabled.
  if ([item action] == @selector(performClose:))
    return YES;

  return [super validateUserInterfaceItem:item];
}

@end
