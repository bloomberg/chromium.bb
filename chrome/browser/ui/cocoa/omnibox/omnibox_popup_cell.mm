// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/omnibox/omnibox_popup_cell.h"

#include <algorithm>
#include <cmath>

#include "base/i18n/rtl.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"

namespace {

// How far to offset image column from the left.
const CGFloat kImageXOffset = 5.0;

// How far to offset the text column from the left.
const CGFloat kTextXOffset = 28.0;

// Rounding radius of selection and hover background on popup items.
const CGFloat kCellRoundingRadius = 2.0;

// Flips the given |rect| in context of the given |frame|.
NSRect FlipIfRTL(NSRect rect, NSRect frame) {
  if (base::i18n::IsRTL()) {
    NSRect result = rect;
    result.origin.x = NSMinX(frame) + (NSMaxX(frame) - NSMaxX(rect));
    return result;
  }
  return rect;
}

// Shifts the left edge of the given |rect| by |dX|
NSRect ShiftRect(NSRect rect, CGFloat dX) {
  NSRect result = rect;
  result.origin.x += dX;
  result.size.width -= dX;
  return result;
}

NSColor* SelectedBackgroundColor() {
  return [NSColor selectedControlColor];
}
NSColor* HoveredBackgroundColor() {
  return [NSColor controlHighlightColor];
}

NSColor* ContentTextColor() {
  return [NSColor blackColor];
}
NSColor* DimTextColor() {
  return [NSColor darkGrayColor];
}
NSColor* URLTextColor() {
  return [NSColor colorWithCalibratedRed:0.0 green:0.55 blue:0.0 alpha:1.0];
}

NSFont* FieldFont() {
  return OmniboxViewMac::GetFieldFont(gfx::Font::NORMAL);
}
NSFont* BoldFieldFont() {
  return OmniboxViewMac::GetFieldFont(gfx::Font::BOLD);
}

NSMutableAttributedString* CreateAttributedString(
    const base::string16& text,
    NSColor* text_color) {
  // Start out with a string using the default style info.
  NSString* s = base::SysUTF16ToNSString(text);
  NSDictionary* attributes = @{
      NSFontAttributeName : FieldFont(),
      NSForegroundColorAttributeName : text_color
  };
  NSMutableAttributedString* as =
      [[[NSMutableAttributedString alloc] initWithString:s
                                              attributes:attributes]
        autorelease];

  NSMutableParagraphStyle* style =
      [[[NSMutableParagraphStyle alloc] init] autorelease];
  [style setLineBreakMode:NSLineBreakByTruncatingTail];
  [style setTighteningFactorForTruncation:0.0];
  [as addAttribute:NSParagraphStyleAttributeName
             value:style
             range:NSMakeRange(0, [as length])];

  return as;
}

NSAttributedString* CreateClassifiedAttributedString(
    const base::string16& text,
    NSColor* text_color,
    const ACMatchClassifications& classifications) {
  NSMutableAttributedString* as = CreateAttributedString(text, text_color);
  NSUInteger match_length = [as length];

  // Mark up the runs which differ from the default.
  for (ACMatchClassifications::const_iterator i = classifications.begin();
       i != classifications.end(); ++i) {
    const bool is_last = ((i + 1) == classifications.end());
    const NSUInteger next_offset =
        (is_last ? match_length : static_cast<NSUInteger>((i + 1)->offset));
    const NSUInteger location = static_cast<NSUInteger>(i->offset);
    const NSUInteger length = next_offset - static_cast<NSUInteger>(i->offset);
    // Guard against bad, off-the-end classification ranges.
    if (location >= match_length || length <= 0)
      break;
    const NSRange range =
        NSMakeRange(location, std::min(length, match_length - location));

    if (0 != (i->style & ACMatchClassification::MATCH)) {
      [as addAttribute:NSFontAttributeName value:BoldFieldFont() range:range];
    }

    if (0 != (i->style & ACMatchClassification::URL)) {
      [as addAttribute:NSForegroundColorAttributeName
                 value:URLTextColor()
                 range:range];
    } else if (0 != (i->style & ACMatchClassification::DIM)) {
      [as addAttribute:NSForegroundColorAttributeName
                 value:DimTextColor()
                 range:range];
    }
  }

  return as;
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

    const base::string16& raw_separator =
        l10n_util::GetStringUTF16(IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR);
    separator_.reset(
        [CreateAttributedString(raw_separator, DimTextColor()) retain]);
  }
  return self;
}

- (void)setMatch:(const AutocompleteMatch&)match {
  match_ = match;
  NSAttributedString *contents = CreateClassifiedAttributedString(
      match_.contents, ContentTextColor(), match_.contents_class);
  [self setAttributedTitle:contents];

  if (match_.description.empty()) {
    description_.reset();
  } else {
    description_.reset([CreateClassifiedAttributedString(
        match_.description, DimTextColor(), match_.description_class) retain]);
  }
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
    [image drawInRect:FlipIfRTL(imageRect, cellFrame)
             fromRect:NSZeroRect  // Entire image
            operation:NSCompositeSourceOver
             fraction:1.0
       respectFlipped:YES
                hints:nil];
  }

  [self drawMatchWithFrame:cellFrame inView:controlView];
}

- (void)drawMatchWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  NSAttributedString* contents = [self attributedTitle];

  CGFloat contentsWidth = [contents size].width;
  CGFloat separatorWidth = [separator_ size].width;
  CGFloat descriptionWidth = description_.get() ? [description_ size].width : 0;
  int contentsMaxWidth, descriptionMaxWidth;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      ceilf(contentsWidth),
      ceilf(separatorWidth),
      ceilf(descriptionWidth),
      ceilf(cellFrame.size.width - kTextXOffset),
      !AutocompleteMatch::IsSearchType(match_.type),
      &contentsMaxWidth,
      &descriptionMaxWidth);

  CGFloat offset = kTextXOffset;
  offset += [self drawMatchPart:contents
                      withFrame:cellFrame
                       atOffset:offset
                   withMaxWidth:contentsMaxWidth
                         inView:controlView];

  if (descriptionMaxWidth != 0) {
    offset += [self drawMatchPart:separator_
                        withFrame:cellFrame
                         atOffset:offset
                     withMaxWidth:separatorWidth
                           inView:controlView];
    offset += [self drawMatchPart:description_
                        withFrame:cellFrame
                         atOffset:offset
                     withMaxWidth:descriptionMaxWidth
                           inView:controlView];
  }
}

- (CGFloat)drawMatchPart:(NSAttributedString*)as
               withFrame:(NSRect)cellFrame
                atOffset:(CGFloat)offset
            withMaxWidth:(int)maxWidth
                  inView:(NSView*)controlView {
  NSRect renderRect = ShiftRect(cellFrame, offset);
  renderRect.size.width =
      std::min(NSWidth(renderRect), static_cast<CGFloat>(maxWidth));
  if (renderRect.size.width != 0) {
    [self drawTitle:as
          withFrame:FlipIfRTL(renderRect, cellFrame)
             inView:controlView];
  }
  return NSWidth(renderRect);
}

@end
