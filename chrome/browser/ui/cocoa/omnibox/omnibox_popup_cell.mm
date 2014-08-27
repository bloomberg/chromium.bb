// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/omnibox/omnibox_popup_cell.h"

#include <algorithm>
#include <cmath>

#include "base/i18n/rtl.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/cocoa/omnibox/omnibox_popup_view_mac.h"
#include "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"

namespace {

// How far to offset image column from the left.
const CGFloat kImageXOffset = 5.0;

// How far to offset the text column from the left.
const CGFloat kTextStartOffset = 28.0;

// Rounding radius of selection and hover background on popup items.
const CGFloat kCellRoundingRadius = 2.0;

// Flips the given |rect| in context of the given |frame|.
NSRect FlipIfRTL(NSRect rect, NSRect frame) {
  DCHECK_LE(NSMinX(frame), NSMinX(rect));
  DCHECK_GE(NSMaxX(frame), NSMaxX(rect));
  if (base::i18n::IsRTL()) {
    NSRect result = rect;
    result.origin.x = NSMinX(frame) + (NSMaxX(frame) - NSMaxX(rect));
    return result;
  }
  return rect;
}

// Shifts the left edge of the given |rect| by |dX|
NSRect ShiftRect(NSRect rect, CGFloat dX) {
  DCHECK_LE(dX, NSWidth(rect));
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

CGFloat GetContentAreaWidth(NSRect cellFrame) {
  return NSWidth(cellFrame) - kTextStartOffset;
}

NSMutableAttributedString* CreateAttributedString(
    const base::string16& text,
    NSColor* text_color,
    NSTextAlignment textAlignment) {
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
  [style setAlignment:textAlignment];
  [as addAttribute:NSParagraphStyleAttributeName
             value:style
             range:NSMakeRange(0, [as length])];

  return as;
}

NSMutableAttributedString* CreateAttributedString(
    const base::string16& text,
    NSColor* text_color) {
  return CreateAttributedString(text, text_color, NSNaturalTextAlignment);
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

- (void)setMaxMatchContentsWidth:(CGFloat)maxMatchContentsWidth {
  maxMatchContentsWidth_ = maxMatchContentsWidth;
}

- (void)setContentsOffset:(CGFloat)contentsOffset {
  contentsOffset_ = contentsOffset;
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

  CGFloat remainingWidth = GetContentAreaWidth(cellFrame);
  CGFloat contentsWidth = [self getMatchContentsWidth];
  CGFloat separatorWidth = [separator_ size].width;
  CGFloat descriptionWidth = description_.get() ? [description_ size].width : 0;
  int contentsMaxWidth, descriptionMaxWidth;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      ceilf(contentsWidth),
      ceilf(separatorWidth),
      ceilf(descriptionWidth),
      ceilf(remainingWidth),
      !AutocompleteMatch::IsSearchType(match_.type),
      &contentsMaxWidth,
      &descriptionMaxWidth);

  CGFloat offset = kTextStartOffset;
  if (match_.type == AutocompleteMatchType::SEARCH_SUGGEST_INFINITE) {
    // Infinite suggestions are rendered with a prefix (usually ellipsis), which
    // appear vertically stacked.
    offset += [self drawMatchPrefixWithFrame:cellFrame
                                      inView:controlView
                        withContentsMaxWidth:&contentsMaxWidth];
  }
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

- (CGFloat)drawMatchPrefixWithFrame:(NSRect)cellFrame
                             inView:(NSView*)controlView
               withContentsMaxWidth:(int*)contentsMaxWidth {
  CGFloat offset = 0.0f;
  CGFloat remainingWidth = GetContentAreaWidth(cellFrame);
  bool isRTL = base::i18n::IsRTL();
  bool isContentsRTL = (base::i18n::RIGHT_TO_LEFT ==
      base::i18n::GetFirstStrongCharacterDirection(match_.contents));
  // Prefix may not have any characters with strong directionality, and may take
  // the UI directionality. But prefix needs to appear in continuation of the
  // contents so we force the directionality.
  NSTextAlignment textAlignment = isContentsRTL ?
      NSRightTextAlignment : NSLeftTextAlignment;
  prefix_.reset([CreateAttributedString(base::UTF8ToUTF16(
      match_.GetAdditionalInfo(kACMatchPropertyContentsPrefix)),
      ContentTextColor(), textAlignment) retain]);
  CGFloat prefixWidth = [prefix_ size].width;

  CGFloat prefixOffset = 0.0f;
  if (isRTL != isContentsRTL) {
    // The contents is rendered between the contents offset extending towards
    // the start edge, while prefix is rendered in opposite direction. Ideally
    // the prefix should be rendered at |contentsOffset_|. If that is not
    // sufficient to render the widest suggestion, we increase it to
    // |maxMatchContentsWidth_|.  If |remainingWidth| is not sufficient to
    // accomodate that, we reduce the offset so that the prefix gets rendered.
    prefixOffset = std::min(
        remainingWidth - prefixWidth, std::max(contentsOffset_,
                                               maxMatchContentsWidth_));
    offset = std::max<CGFloat>(0.0, prefixOffset - *contentsMaxWidth);
  } else { // The direction of contents is same as UI direction.
    // Ideally the offset should be |contentsOffset_|. If the max total width
    // (|prefixWidth| + |maxMatchContentsWidth_|) from offset will exceed the
    // |remainingWidth|, then we shift the offset to the left , so that all
    // postfix suggestions are visible.
    // We have to render the prefix, so offset has to be at least |prefixWidth|.
    offset = std::max(prefixWidth,
        std::min(remainingWidth - maxMatchContentsWidth_, contentsOffset_));
    prefixOffset = offset - prefixWidth;
  }
  *contentsMaxWidth = std::min((int)ceilf(remainingWidth - prefixWidth),
                               *contentsMaxWidth);
  [self drawMatchPart:prefix_
            withFrame:cellFrame
             atOffset:prefixOffset + kTextStartOffset
         withMaxWidth:prefixWidth
               inView:controlView];
  return offset;
}

- (CGFloat)drawMatchPart:(NSAttributedString*)as
               withFrame:(NSRect)cellFrame
                atOffset:(CGFloat)offset
            withMaxWidth:(int)maxWidth
                  inView:(NSView*)controlView {
  if (offset > NSWidth(cellFrame))
    return 0.0f;
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

- (CGFloat)getMatchContentsWidth {
  NSAttributedString* contents = [self attributedTitle];
  return contents ? [contents size].width : 0;
}


+ (CGFloat)computeContentsOffset:(const AutocompleteMatch&)match {
  const base::string16& inputText = base::UTF8ToUTF16(
      match.GetAdditionalInfo(kACMatchPropertyInputText));
  int contentsStartIndex = 0;
  base::StringToInt(
      match.GetAdditionalInfo(kACMatchPropertyContentsStartIndex),
      &contentsStartIndex);
  // Ignore invalid state.
  if (!StartsWith(match.fill_into_edit, inputText, true)
      || !EndsWith(match.fill_into_edit, match.contents, true)
      || ((size_t)contentsStartIndex >= inputText.length())) {
    return 0;
  }
  bool isRTL = base::i18n::IsRTL();
  bool isContentsRTL = (base::i18n::RIGHT_TO_LEFT ==
      base::i18n::GetFirstStrongCharacterDirection(match.contents));

  // Color does not matter.
  NSAttributedString* as = CreateAttributedString(inputText, DimTextColor());
  base::scoped_nsobject<NSTextStorage> textStorage([[NSTextStorage alloc]
      initWithAttributedString:as]);
  base::scoped_nsobject<NSLayoutManager> layoutManager(
      [[NSLayoutManager alloc] init]);
  base::scoped_nsobject<NSTextContainer> textContainer(
      [[NSTextContainer alloc] init]);
  [layoutManager addTextContainer:textContainer];
  [textStorage addLayoutManager:layoutManager];

  NSUInteger charIndex = static_cast<NSUInteger>(contentsStartIndex);
  NSUInteger glyphIndex =
      [layoutManager glyphIndexForCharacterAtIndex:charIndex];

  // This offset is computed from the left edge of the glyph always from the
  // left edge of the string, irrespective of the directionality of UI or text.
  CGFloat glyphOffset = [layoutManager locationForGlyphAtIndex:glyphIndex].x;

  CGFloat inputWidth = [as size].width;

  // The offset obtained above may need to be corrected because the left-most
  // glyph may not have 0 offset. So we find the offset of left-most glyph, and
  // subtract it from the offset of the glyph we obtained above.
  CGFloat minOffset = glyphOffset;

  // If content is RTL, we are interested in the right-edge of the glyph.
  // Unfortunately the bounding rect computation methods from NSLayoutManager or
  // NSFont don't work correctly with bidirectional text. So we compute the
  // glyph width by finding the closest glyph offset to the right of the glyph
  // we are looking for.
  CGFloat glyphWidth = inputWidth;

  for (NSUInteger i = 0; i < [as length]; i++) {
    if (i == charIndex) continue;
    glyphIndex = [layoutManager glyphIndexForCharacterAtIndex:i];
    CGFloat offset = [layoutManager locationForGlyphAtIndex:glyphIndex].x;
    minOffset = std::min(minOffset, offset);
    if (offset > glyphOffset)
      glyphWidth = std::min(glyphWidth, offset - glyphOffset);
  }
  glyphOffset -= minOffset;
  if (glyphWidth == 0)
    glyphWidth = inputWidth - glyphOffset;
  if (isContentsRTL)
    glyphOffset += glyphWidth;
  return isRTL ? (inputWidth - glyphOffset) : glyphOffset;
}

@end
