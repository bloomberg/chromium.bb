// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_bar_toolbar_view.h"

#import "chrome/browser/cocoa/bookmark_bar_controller.h"

@implementation BookmarkBarToolbarView

- (void)drawRect:(NSRect)rect {
  if ([controller_ drawAsFloatingBar]) {
    // TODO(erg): Deal with themes. http://crbug.com/17625
    [[NSColor colorWithDeviceRed:1.0 green:1.0 blue:1.0 alpha:1.0] set];
    NSRectFill(rect);
  } else {
    [super drawRect:rect];
  }
}

@end  // @implementation BookmarkBarToolbarView
