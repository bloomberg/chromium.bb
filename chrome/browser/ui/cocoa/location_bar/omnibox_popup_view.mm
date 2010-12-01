// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/omnibox_popup_view.h"

#include "base/logging.h"

@implementation OmniboxPopupView

// If there is only one subview, it is sized to fill all available space.  If
// there are two subviews, the second subview is placed at the bottom of the
// view, and the first subview is sized to fill all remaining space.
- (void)resizeSubviewsWithOldSize:(NSSize)oldBoundsSize {
  NSArray* subviews = [self subviews];
  if ([subviews count] == 0)
    return;

  DCHECK_LE([subviews count], 2U);

  NSRect availableSpace = [self bounds];

  if ([subviews count] >= 2) {
    NSView* instantView = [subviews objectAtIndex:1];
    CGFloat height = NSHeight([instantView frame]);
    NSRect instantFrame = availableSpace;
    instantFrame.size.height = height;

    availableSpace.origin.y = height;
    availableSpace.size.height -= height;
    [instantView setFrame:instantFrame];
  }

  if ([subviews count] >= 1) {
    NSView* matrixView = [subviews objectAtIndex:0];
    if (NSHeight(availableSpace) < 0)
      availableSpace.size.height = 0;

    [matrixView setFrame:availableSpace];
  }
}

@end
