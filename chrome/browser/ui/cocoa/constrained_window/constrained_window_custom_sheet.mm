// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"

#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_animation.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"

@implementation CustomConstrainedWindowSheet

- (id)initWithCustomWindow:(NSWindow*)customWindow {
  if ((self = [super init])) {
    customWindow_.reset([customWindow retain]);
  }
  return self;
}

- (void)showSheetForWindow:(NSWindow*)window {
  base::scoped_nsobject<NSAnimation> animation(
      [[ConstrainedWindowAnimationShow alloc] initWithWindow:customWindow_]);
  [window addChildWindow:customWindow_
                 ordered:NSWindowAbove];
  [self unhideSheet];
  [self updateSheetPosition];
  [customWindow_ makeKeyAndOrderFront:nil];
  [animation startAnimation];
}

- (void)closeSheetWithAnimation:(BOOL)withAnimation {
  if (withAnimation) {
    base::scoped_nsobject<NSAnimation> animation(
        [[ConstrainedWindowAnimationHide alloc] initWithWindow:customWindow_]);
    [animation startAnimation];
  }

  [[customWindow_ parentWindow] removeChildWindow:customWindow_];
  [customWindow_ close];
}

- (void)hideSheet {
  // Hide the sheet window, and any of its direct child windows, by setting the
  // alpha to 0. This technique is used instead of -orderOut: because that may
  // cause a Spaces change or window ordering change.
  [customWindow_ setAlphaValue:0.0];
  for (NSWindow* window : [customWindow_ childWindows]) {
    [window setAlphaValue:0.0];
  }
}

- (void)unhideSheet {
  [customWindow_ setAlphaValue:1.0];
  for (NSWindow* window : [customWindow_ childWindows]) {
    [window setAlphaValue:1.0];
  }
}

- (void)pulseSheet {
  base::scoped_nsobject<NSAnimation> animation(
      [[ConstrainedWindowAnimationPulse alloc] initWithWindow:customWindow_]);
  [animation startAnimation];
}

- (void)makeSheetKeyAndOrderFront {
  [customWindow_ makeKeyAndOrderFront:nil];
}

- (void)updateSheetPosition {
  ConstrainedWindowSheetController* controller =
      [ConstrainedWindowSheetController controllerForSheet:self];
  NSPoint origin = [controller originForSheet:self
                               withWindowSize:[customWindow_ frame].size];
  [customWindow_ setFrameOrigin:origin];
}

@end
