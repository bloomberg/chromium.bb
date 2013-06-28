// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/omnibox/omnibox_popup_cell.h"

#include <cmath>

namespace {

// How far to offset image column from the left.
const CGFloat kImageXOffset = 5.0;

// How far to offset the text column from the left.
const CGFloat kTextXOffset = 28.0;

// Rounding radius of selection and hover background on popup items.
const CGFloat kCellRoundingRadius = 2.0;

NSColor* SelectedBackgroundColor() {
  return [NSColor selectedControlColor];
}
NSColor* HoveredBackgroundColor() {
  return [NSColor controlHighlightColor];
}

}  // namespace

@implementation OmniboxPopupCell

- (id)init {
  self = [super init];
  if (self) {
    [self setImagePosition:NSImageLeft];
    [self setBordered:NO];
    [self setButtonType:NSRadioButton];

    // Without this highlighting messes up white areas of images.
    [self setHighlightsBy:NSNoCellMask];
  }
  return self;
}

// The default NSButtonCell drawing leaves the image flush left and
// the title next to the image.  This spaces things out to line up
// with the star button and autocomplete field.
- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
  if ([self state] == NSOnState || [self isHighlighted]) {
    if ([self state] == NSOnState)
      [SelectedBackgroundColor() set];
    else
      [HoveredBackgroundColor() set];
    NSBezierPath* path =
        [NSBezierPath bezierPathWithRoundedRect:cellFrame
                                        xRadius:kCellRoundingRadius
                                        yRadius:kCellRoundingRadius];
    [path fill];
  }

  // Put the image centered vertically but in a fixed column.
  NSImage* image = [self image];
  if (image) {
    NSRect imageRect = cellFrame;
    imageRect.size = [image size];
    imageRect.origin.y +=
        std::floor((NSHeight(cellFrame) - NSHeight(imageRect)) / 2.0);
    imageRect.origin.x += kImageXOffset;
    [image drawInRect:imageRect
             fromRect:NSZeroRect  // Entire image
            operation:NSCompositeSourceOver
             fraction:1.0
       respectFlipped:YES
                hints:nil];
  }

  // Adjust the title position to be lined up under the field's text.
  NSAttributedString* title = [self attributedTitle];
  if (title && [title length]) {
    NSRect titleRect = cellFrame;
    titleRect.size.width -= kTextXOffset;
    titleRect.origin.x += kTextXOffset;
    [self drawTitle:title withFrame:titleRect inView:controlView];
  }
}

@end
