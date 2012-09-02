// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_info.h"
#include "ui/base/cocoa/window_size_constants.h"

@implementation ConstrainedWindowSheetInfo

- (id)initWithSheet:(NSWindow*)sheet
         parentView:(NSView*)parentView
      overlayWindow:(NSWindow*)overlayWindow {
  if ((self = [super init])) {
    sheet_.reset([sheet retain]);
    parentView_.reset([parentView retain]);
    overlayWindow_.reset([overlayWindow retain]);
  }
  return self;
}

- (NSWindow*)sheet {
  return sheet_;
}

- (NSView*)parentView {
  return parentView_;
}

- (NSWindow*)overlayWindow {
  return overlayWindow_;
}

- (void)setAnimation:(NSAnimation*)animation {
  animation_.reset([animation retain]);
}

- (NSAnimation*)animation {
  return animation_;
}

- (void)hideSheet {
  // Stop any pending animations.
  [animation_ stopAnimation];
  animation_.reset();

  // Hide the sheet by setting alpha to 0 and sizing it to 1x1. This is better
  // than calling orderOut: because that could cause Spaces activation or
  // window ordering changes.
  [sheet_ setAlphaValue:0.0];
  oldSheetAutoresizesSubviews_ = [[sheet_ contentView] autoresizesSubviews];
  [[sheet_ contentView] setAutoresizesSubviews:NO];
  NSRect overlayFrame = [overlayWindow_ frame];
  oldSheetFrame_ = NSOffsetRect(
      [sheet_ frame], -overlayFrame.origin.x, -overlayFrame.origin.y);
  [sheet_ setFrame:ui::kWindowSizeDeterminedLater display:NO];

  // Overlay window is already invisible so just stop accepting mouse events.
  [overlayWindow_ setIgnoresMouseEvents:YES];

  // Make sure the now invisible sheet doesn't keep keyboard focus
  [[overlayWindow_ parentWindow] makeKeyWindow];
}

- (void)showSheet {
  [overlayWindow_ setIgnoresMouseEvents:NO];

  NSRect overlayFrame = [overlayWindow_ frame];
  NSRect newSheetFrame = NSOffsetRect(
      oldSheetFrame_, overlayFrame.origin.x, overlayFrame.origin.y);
  [sheet_ setFrame:newSheetFrame display:NO];
  [[sheet_ contentView] setAutoresizesSubviews:oldSheetAutoresizesSubviews_];
  [sheet_ setAlphaValue:1.0];

  [overlayWindow_ makeKeyWindow];
}

@end
