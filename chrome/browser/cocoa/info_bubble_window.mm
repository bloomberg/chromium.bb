// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/info_bubble_window.h"

@implementation InfoBubbleWindow

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)aStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)flag {
  if ((self = [super initWithContentRect:contentRect
                               styleMask:NSBorderlessWindowMask
                                 backing:bufferingType
                                   defer:flag])) {
    [self setBackgroundColor:[NSColor clearColor]];
    [self setExcludedFromWindowsMenu:YES];
    [self setAlphaValue:1.0];
    [self setOpaque:NO];
    [self setHasShadow:YES];
  }
  return self;
}

// According to
// http://www.cocoabuilder.com/archive/message/cocoa/2006/6/19/165953,
// NSBorderlessWindowMask windows cannot become key or main. In our
// case, however, we don't want all of that behavior. As an example,
// our bubble could have buttons.

- (BOOL)canBecomeKeyWindow {
  return YES;
}

@end
