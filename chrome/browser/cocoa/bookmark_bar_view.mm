// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/bookmark_bar_view.h"

@implementation BookmarkBarView

// Only hit in a unit test.
- (void)setContextMenu:(NSMenu*)menu {
  barContextualMenu_ = menu;
}

// Unlike controls, generic views don't have a well-defined context
// menu (e.g. responds to the "menu" selector).  So we add our own.
- (NSMenu *)menuForEvent:(NSEvent *)event {
  if ([event type] == NSRightMouseDown)
    return barContextualMenu_;
  return nil;
}

@end
