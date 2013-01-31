// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/separator_decoration.h"

#import "chrome/browser/ui/cocoa/nsview_additions.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"

SeparatorDecoration::SeparatorDecoration() {
}

SeparatorDecoration::~SeparatorDecoration() {
}

void SeparatorDecoration::DrawInFrame(NSRect frame, NSView* control_view) {
  // Inset by 1 from top and bottom to avoid drawing on top of the the omnibox
  // bezel.
  NSRect rect = NSInsetRect(frame, 0, 1);
  rect.size.width = [control_view cr_lineWidth];
  rect.origin.x = NSMaxX(frame) - NSWidth(rect);
  [SeparatorColor(control_view) set];
  NSRectFillUsingOperation(rect, NSCompositeSourceOver);
}

CGFloat SeparatorDecoration::GetWidthForSpace(CGFloat width,
                                              CGFloat text_width) {
  return 2;
}

bool SeparatorDecoration::IsSeparator() const {
  return true;
}

NSColor* SeparatorDecoration::SeparatorColor(NSView* view) const {
  CGFloat alpha = 0.25;
  if (view && [view cr_lineWidth] != 1.0)
    alpha = 0.5;
  return [OmniboxViewMac::SuggestTextColor() colorWithAlphaComponent:alpha];
}
