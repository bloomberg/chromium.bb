// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/omnibox/omnibox_popup_cell.h"

#include <algorithm>
#include <cmath>

#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace {

// How far to offset image column from the left.
const CGFloat kImageXOffset = 5.0;

// How far to offset the text column from the left.
const CGFloat kTextXOffset = 28.0;

// Maximum fraction of the popup width that can be used to display match
// contents.
const CGFloat kMinDescriptionFraction = 0.7;

// Rounding radius of selection and hover background on popup items.
const CGFloat kCellRoundingRadius = 2.0;

void DrawFadeTruncatingTitle(NSAttributedString* title,
                             NSRect titleRect,
                             NSColor* backgroundColor) {
  gfx::ScopedNSGraphicsContextSaveGState scopedGState;
  NSRectClip(titleRect);

  // Draw the entire text.
  NSSize textSize = [title size];
  NSPoint textOrigin = titleRect.origin;
  textOrigin.y += roundf((NSHeight(titleRect) - textSize.height) / 2.0) - 1.0;
  [title drawAtPoint:textOrigin];

  // Empirically, Cocoa will draw an extra 2 pixels past NSWidth(titleRect)
  // before it clips the text.
  const CGFloat kOverflowBeforeClip = 2.0;
  CGFloat clipped_width = NSWidth(titleRect) + kOverflowBeforeClip;
  if (textSize.width <= clipped_width)
    return;

  // The gradient width is the same as the line height.
  CGFloat gradientWidth = std::min(textSize.height, NSWidth(titleRect) / 4);

  // Draw the gradient part.
  NSColor *alphaColor = [backgroundColor colorWithAlphaComponent:0.0];
  base::scoped_nsobject<NSGradient> mask(
      [[NSGradient alloc] initWithStartingColor:alphaColor
                                    endingColor:backgroundColor]);
  [mask drawFromPoint:NSMakePoint(NSMaxX(titleRect) - gradientWidth,
                                  NSMinY(titleRect))
              toPoint:NSMakePoint(NSMaxX(titleRect),
                                  NSMinY(titleRect))
              options:NSGradientDrawsBeforeStartingLocation];
}

}  // namespace

@implementation OmniboxPopupCell

- (id)init {
  if ((self = [super init])) {
    [self setImagePosition:NSImageLeft];
    [self setBordered:NO];
    [self setButtonType:NSRadioButton];

    // Without this highlighting messes up white areas of images.
    [self setHighlightsBy:NSNoCellMask];
  }
  return self;
}

- (void)setContentText:(NSAttributedString*)contentText {
  [[self controlView] setNeedsDisplay:YES];
  contentText_.reset([contentText retain]);
}

- (void)setDescriptionText:(NSAttributedString*)descriptionText {
  [[self controlView] setNeedsDisplay:YES];
  if (![descriptionText length]) {
    descriptionText_.reset();
    return;
  }

  base::scoped_nsobject<NSMutableAttributedString> dashDescriptionText(
      [[NSMutableAttributedString alloc]
          initWithAttributedString:descriptionText]);
  NSString* rawEnDash = @" \u2013 ";
  [dashDescriptionText replaceCharactersInRange:NSMakeRange(0, 0)
                                     withString:rawEnDash];
  descriptionText_.reset(dashDescriptionText.release());
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
  NSColor* backgroundColor = [NSColor controlBackgroundColor];
  if ([self state] == NSOnState || [self isHighlighted]) {
    if ([self state] == NSOnState)
      backgroundColor = [NSColor selectedControlColor];
    else
      backgroundColor = [NSColor controlHighlightColor];
    [backgroundColor set];
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
  if ([contentText_ length]) {
    NSRect availRect = cellFrame;
    availRect.size.width = NSWidth(cellFrame) - kTextXOffset;
    availRect.origin.x += kTextXOffset;
    CGFloat availWidth = NSWidth(availRect);
    CGFloat contentWidth = [contentText_ size].width;
    CGFloat descWidth =
        [descriptionText_ length] ? [descriptionText_ size].width : 0;

    CGFloat tempDescWidth =
        std::min(descWidth, kMinDescriptionFraction * availWidth);
    contentWidth = std::min(contentWidth, availWidth - tempDescWidth);

    NSRect contentRect;
    NSRect descRect;
    NSDivideRect(
        availRect, &contentRect, &descRect, contentWidth, NSMinXEdge);

    DrawFadeTruncatingTitle(contentText_, contentRect, backgroundColor);
    if ([descriptionText_ length])
      DrawFadeTruncatingTitle(descriptionText_, descRect, backgroundColor);
  }
}

@end
