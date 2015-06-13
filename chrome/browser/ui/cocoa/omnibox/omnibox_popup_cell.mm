// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/omnibox/omnibox_popup_cell.h"

#include <algorithm>
#include <cmath>

#include "base/i18n/rtl.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/cocoa/omnibox/omnibox_popup_view_mac.h"
#include "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/suggestion_answer.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"

namespace {

// How much to adjust the cell sizing up from the default determined
// by the font.
const CGFloat kCellHeightAdjust = 6.0;

// How large the icon should be when displayed.
const CGFloat kImageSize = 19.0;

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
NSColor* PositiveTextColor() {
  return gfx::SkColorToCalibratedNSColor(SkColorSetRGB(0x0b, 0x80, 0x43));
}
NSColor* NegativeTextColor() {
  return gfx::SkColorToCalibratedNSColor(SkColorSetRGB(0xc5, 0x39, 0x29));
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
NSFont* LargeFont() {
  return OmniboxViewMac::GetLargeFont(gfx::Font::NORMAL);
}
NSFont* LargeSuperscriptFont() {
  NSFont* font = OmniboxViewMac::GetLargeFont(gfx::Font::NORMAL);
  // Calculate a slightly smaller font. The ratio here is somewhat arbitrary.
  // Proportions from 5/9 to 5/7 all look pretty good.
  CGFloat size = [font pointSize] * 5.0 / 9.0;
  NSFontDescriptor* descriptor = [font fontDescriptor];
  return [NSFont fontWithDescriptor:descriptor size:size];
}
NSFont* SmallFont() {
  return OmniboxViewMac::GetSmallFont(gfx::Font::NORMAL);
}

CGFloat GetContentAreaWidth(NSRect cellFrame) {
  return NSWidth(cellFrame) - kTextStartOffset;
}

NSAttributedString* CreateAnswerString(const base::string16& text,
                                       NSInteger style_type) {
  NSDictionary* answer_style = nil;
  switch (style_type) {
    case SuggestionAnswer::ANSWER:
      answer_style = @{
        NSForegroundColorAttributeName : ContentTextColor(),
        NSFontAttributeName : LargeFont()
      };
      break;
    case SuggestionAnswer::HEADLINE:
      answer_style = @{
        NSForegroundColorAttributeName : DimTextColor(),
        NSFontAttributeName : LargeFont()
      };
      break;
    case SuggestionAnswer::TOP_ALIGNED:
      answer_style = @{
        NSForegroundColorAttributeName : DimTextColor(),
        NSFontAttributeName : LargeSuperscriptFont(),
        NSSuperscriptAttributeName : @1
      };
      break;
    case SuggestionAnswer::DESCRIPTION:
      answer_style = @{
        NSForegroundColorAttributeName : DimTextColor(),
        NSFontAttributeName : FieldFont()
      };
      break;
    case SuggestionAnswer::DESCRIPTION_NEGATIVE:
      answer_style = @{
        NSForegroundColorAttributeName : NegativeTextColor(),
        NSFontAttributeName : LargeSuperscriptFont()
      };
      break;
    case SuggestionAnswer::DESCRIPTION_POSITIVE:
      answer_style = @{
        NSForegroundColorAttributeName : PositiveTextColor(),
        NSFontAttributeName : LargeSuperscriptFont()
      };
      break;
    case SuggestionAnswer::MORE_INFO:
      answer_style = @{
        NSForegroundColorAttributeName : DimTextColor(),
        NSFontAttributeName : SmallFont()
      };
      break;
    case SuggestionAnswer::SUGGESTION:
      answer_style = @{
        NSForegroundColorAttributeName : ContentTextColor(),
        NSFontAttributeName : FieldFont()
      };
      break;
    case SuggestionAnswer::SUGGESTION_POSITIVE:
      answer_style = @{
        NSForegroundColorAttributeName : PositiveTextColor(),
        NSFontAttributeName : FieldFont()
      };
      break;
    case SuggestionAnswer::SUGGESTION_NEGATIVE:
      answer_style = @{
        NSForegroundColorAttributeName : NegativeTextColor(),
        NSFontAttributeName : FieldFont()
      };
      break;
    case SuggestionAnswer::SUGGESTION_LINK:
      answer_style = @{
        NSForegroundColorAttributeName : URLTextColor(),
        NSFontAttributeName : FieldFont()
      };
      break;
    case SuggestionAnswer::STATUS:
      answer_style = @{
        NSForegroundColorAttributeName : DimTextColor(),
        NSFontAttributeName : LargeSuperscriptFont()
      };
      break;
    case SuggestionAnswer::PERSONALIZED_SUGGESTION:
      answer_style = @{
        NSForegroundColorAttributeName : ContentTextColor(),
        NSFontAttributeName : FieldFont()
      };
      break;
  }

  // Start out with a string using the default style info.
  base::scoped_nsobject<NSMutableAttributedString> attributed_string(
      [[NSMutableAttributedString alloc]
          initWithString:base::SysUTF16ToNSString(text)
              attributes:answer_style]);

  base::scoped_nsobject<NSMutableParagraphStyle> style(
      [[NSMutableParagraphStyle alloc] init]);
  [style setLineBreakMode:NSLineBreakByTruncatingTail];
  [style setTighteningFactorForTruncation:0.0];
  [attributed_string addAttribute:NSParagraphStyleAttributeName
                            value:style
                            range:NSMakeRange(0, [attributed_string length])];
  return attributed_string.autorelease();
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
  NSMutableAttributedString* attributedString = [[
      [NSMutableAttributedString alloc] initWithString:s
                                            attributes:attributes] autorelease];

  NSMutableParagraphStyle* style =
      [[[NSMutableParagraphStyle alloc] init] autorelease];
  [style setLineBreakMode:NSLineBreakByTruncatingTail];
  [style setTighteningFactorForTruncation:0.0];
  [style setAlignment:textAlignment];
  [attributedString addAttribute:NSParagraphStyleAttributeName
                           value:style
                           range:NSMakeRange(0, [attributedString length])];

  return attributedString;
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
  NSMutableAttributedString* attributedString =
      CreateAttributedString(text, text_color);
  NSUInteger match_length = [attributedString length];

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
      [attributedString addAttribute:NSFontAttributeName
                               value:BoldFieldFont()
                               range:range];
    }

    if (0 != (i->style & ACMatchClassification::URL)) {
      [attributedString addAttribute:NSForegroundColorAttributeName
                               value:URLTextColor()
                               range:range];
    } else if (0 != (i->style & ACMatchClassification::DIM)) {
      [attributedString addAttribute:NSForegroundColorAttributeName
                               value:DimTextColor()
                               range:range];
    }
  }

  return attributedString;
}

}  // namespace

@interface OmniboxPopupCell ()
- (CGFloat)drawMatchPart:(NSAttributedString*)attributedString
               withFrame:(NSRect)cellFrame
                atOffset:(CGFloat)offset
            withMaxWidth:(int)maxWidth;
- (CGFloat)drawMatchPrefixWithFrame:(NSRect)cellFrame
                          tableView:(OmniboxPopupMatrix*)tableView
               withContentsMaxWidth:(int*)contentsMaxWidth;
- (void)drawMatchWithFrame:(NSRect)cellFrame inView:(NSView*)controlView;
@end

@implementation OmniboxPopupCellData

@synthesize contents = contents_;
@synthesize description = description_;
@synthesize prefix = prefix_;
@synthesize image = image_;
@synthesize answerImage = answerImage_;
@synthesize contentsOffset = contentsOffset_;
@synthesize isContentsRTL = isContentsRTL_;
@synthesize matchType = matchType_;

- (instancetype)initWithMatch:(const AutocompleteMatch&)match
               contentsOffset:(CGFloat)contentsOffset
                        image:(NSImage*)image
                  answerImage:(NSImage*)answerImage {
  if ((self = [super init])) {
    image_ = [image retain];
    answerImage_ = [answerImage retain];
    contentsOffset_ = contentsOffset;

    isContentsRTL_ =
        (base::i18n::RIGHT_TO_LEFT ==
         base::i18n::GetFirstStrongCharacterDirection(match.contents));
    matchType_ = match.type;

    // Prefix may not have any characters with strong directionality, and may
    // take the UI directionality. But prefix needs to appear in continuation
    // of the contents so we force the directionality.
    NSTextAlignment textAlignment =
        isContentsRTL_ ? NSRightTextAlignment : NSLeftTextAlignment;
    prefix_ =
        [CreateAttributedString(base::UTF8ToUTF16(match.GetAdditionalInfo(
                                    kACMatchPropertyContentsPrefix)),
                                ContentTextColor(), textAlignment) retain];

    contents_ = [CreateClassifiedAttributedString(
        match.contents, ContentTextColor(), match.contents_class) retain];

    if (match.answer) {
      base::scoped_nsobject<NSMutableAttributedString> answerString(
          [[NSMutableAttributedString alloc] init]);
      DCHECK(!match.answer->second_line().text_fields().empty());
      for (const SuggestionAnswer::TextField& textField :
           match.answer->second_line().text_fields()) {
        [answerString
            appendAttributedString:CreateAnswerString(textField.text(),
                                                      textField.type())];
      }
      const base::string16 space(base::ASCIIToUTF16(" "));
      const SuggestionAnswer::TextField* textField =
          match.answer->second_line().additional_text();
      if (textField) {
        [answerString
            appendAttributedString:CreateAnswerString(space + textField->text(),
                                                      textField->type())];
      }
      textField = match.answer->second_line().status_text();
      if (textField) {
        [answerString
            appendAttributedString:CreateAnswerString(space + textField->text(),
                                                      textField->type())];
      }
      description_ = answerString.release();
    } else if (!match.description.empty()) {
      description_ = [CreateClassifiedAttributedString(
          match.description, DimTextColor(), match.description_class) retain];
    }
  }
  return self;
}

- (instancetype)copyWithZone:(NSZone*)zone {
  return [self retain];
}

- (CGFloat)getMatchContentsWidth {
  return [contents_ size].width;
}

- (CGFloat)rowHeight {
  return kImageSize + kCellHeightAdjust;
}

@end

@implementation OmniboxPopupCell

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
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

  [self drawMatchWithFrame:cellFrame inView:controlView];
}

- (void)drawMatchWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  OmniboxPopupCellData* cellData =
      base::mac::ObjCCastStrict<OmniboxPopupCellData>([self objectValue]);
  OmniboxPopupMatrix* tableView =
      base::mac::ObjCCastStrict<OmniboxPopupMatrix>(controlView);
  CGFloat remainingWidth = GetContentAreaWidth(cellFrame);
  CGFloat contentsWidth = [cellData getMatchContentsWidth];
  CGFloat separatorWidth = [[tableView separator] size].width;
  CGFloat descriptionWidth =
      [cellData description] ? [[cellData description] size].width : 0;
  int contentsMaxWidth, descriptionMaxWidth;
  OmniboxPopupModel::ComputeMatchMaxWidths(
      ceilf(contentsWidth), ceilf(separatorWidth), ceilf(descriptionWidth),
      ceilf(remainingWidth),
      !AutocompleteMatch::IsSearchType([cellData matchType]), &contentsMaxWidth,
      &descriptionMaxWidth);

  // Put the image centered vertically but in a fixed column.
  NSRect imageRect = cellFrame;
  imageRect.size = [[cellData image] size];
  imageRect.origin.y +=
      std::floor((NSHeight(cellFrame) - NSHeight(imageRect)) / 2.0);
  imageRect.origin.x += kImageXOffset;
  [[cellData image] drawInRect:FlipIfRTL(imageRect, cellFrame)
                      fromRect:NSZeroRect
                     operation:NSCompositeSourceOver
                      fraction:1.0
                respectFlipped:YES
                         hints:nil];

  CGFloat offset = kTextStartOffset;
  if ([cellData matchType] == AutocompleteMatchType::SEARCH_SUGGEST_TAIL) {
    // Infinite suggestions are rendered with a prefix (usually ellipsis), which
    // appear vertically stacked.
    offset += [self drawMatchPrefixWithFrame:cellFrame
                                   tableView:tableView
                        withContentsMaxWidth:&contentsMaxWidth];
  }
  offset += [self drawMatchPart:[cellData contents]
                      withFrame:cellFrame
                       atOffset:offset
                   withMaxWidth:contentsMaxWidth];

  if (descriptionMaxWidth != 0) {
    offset += [self drawMatchPart:[tableView separator]
                        withFrame:cellFrame
                         atOffset:offset
                     withMaxWidth:separatorWidth];
      NSRect imageRect = NSMakeRect(offset, NSMinY(cellFrame),
          NSHeight(cellFrame), NSHeight(cellFrame));
      [[cellData answerImage] drawInRect:FlipIfRTL(imageRect, cellFrame)
                                fromRect:NSZeroRect
                               operation:NSCompositeSourceOver
                                fraction:1.0
                          respectFlipped:YES
                                   hints:nil];
      if ([cellData answerImage])
        offset += NSWidth(imageRect);
      offset += [self drawMatchPart:[cellData description]
                          withFrame:cellFrame
                           atOffset:offset
                       withMaxWidth:descriptionMaxWidth];
  }
}

- (CGFloat)drawMatchPrefixWithFrame:(NSRect)cellFrame
                          tableView:(OmniboxPopupMatrix*)tableView
               withContentsMaxWidth:(int*)contentsMaxWidth {
  OmniboxPopupCellData* cellData =
      base::mac::ObjCCastStrict<OmniboxPopupCellData>([self objectValue]);
  CGFloat offset = 0.0f;
  CGFloat remainingWidth = GetContentAreaWidth(cellFrame);
  CGFloat prefixWidth = [[cellData prefix] size].width;

  CGFloat prefixOffset = 0.0f;
  if (base::i18n::IsRTL() != [cellData isContentsRTL]) {
    // The contents is rendered between the contents offset extending towards
    // the start edge, while prefix is rendered in opposite direction. Ideally
    // the prefix should be rendered at |contentsOffset_|. If that is not
    // sufficient to render the widest suggestion, we increase it to
    // |maxMatchContentsWidth|.  If |remainingWidth| is not sufficient to
    // accommodate that, we reduce the offset so that the prefix gets rendered.
    prefixOffset = std::min(
        remainingWidth - prefixWidth,
        std::max([cellData contentsOffset], [tableView maxMatchContentsWidth]));
    offset = std::max<CGFloat>(0.0, prefixOffset - *contentsMaxWidth);
  } else { // The direction of contents is same as UI direction.
    // Ideally the offset should be |contentsOffset_|. If the max total width
    // (|prefixWidth| + |maxMatchContentsWidth|) from offset will exceed the
    // |remainingWidth|, then we shift the offset to the left , so that all
    // postfix suggestions are visible.
    // We have to render the prefix, so offset has to be at least |prefixWidth|.
    offset =
        std::max(prefixWidth,
                 std::min(remainingWidth - [tableView maxMatchContentsWidth],
                          [cellData contentsOffset]));
    prefixOffset = offset - prefixWidth;
  }
  *contentsMaxWidth = std::min((int)ceilf(remainingWidth - prefixWidth),
                               *contentsMaxWidth);
  [self drawMatchPart:[cellData prefix]
            withFrame:cellFrame
             atOffset:prefixOffset + kTextStartOffset
         withMaxWidth:prefixWidth];
  return offset;
}

- (CGFloat)drawMatchPart:(NSAttributedString*)attributedString
               withFrame:(NSRect)cellFrame
                atOffset:(CGFloat)offset
            withMaxWidth:(int)maxWidth {
  if (offset > NSWidth(cellFrame))
    return 0.0f;
  NSRect renderRect = ShiftRect(cellFrame, offset);
  renderRect.size.width =
      std::min(NSWidth(renderRect), static_cast<CGFloat>(maxWidth));
  if (NSWidth(renderRect) > 0.0)
    [attributedString drawInRect:FlipIfRTL(renderRect, cellFrame)];
  return NSWidth(renderRect);
}

+ (CGFloat)computeContentsOffset:(const AutocompleteMatch&)match {
  const base::string16& inputText = base::UTF8ToUTF16(
      match.GetAdditionalInfo(kACMatchPropertyInputText));
  int contentsStartIndex = 0;
  base::StringToInt(
      match.GetAdditionalInfo(kACMatchPropertyContentsStartIndex),
      &contentsStartIndex);
  // Ignore invalid state.
  if (!base::StartsWith(match.fill_into_edit, inputText, true) ||
      !base::EndsWith(match.fill_into_edit, match.contents, true) ||
      ((size_t)contentsStartIndex >= inputText.length())) {
    return 0;
  }
  bool isContentsRTL = (base::i18n::RIGHT_TO_LEFT ==
      base::i18n::GetFirstStrongCharacterDirection(match.contents));

  // Color does not matter.
  NSAttributedString* attributedString =
      CreateAttributedString(inputText, DimTextColor());
  base::scoped_nsobject<NSTextStorage> textStorage(
      [[NSTextStorage alloc] initWithAttributedString:attributedString]);
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

  CGFloat inputWidth = [attributedString size].width;

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

  for (NSUInteger i = 0; i < [attributedString length]; i++) {
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
  return base::i18n::IsRTL() ? (inputWidth - glyphOffset) : glyphOffset;
}

+ (NSAttributedString*)createSeparatorString {
  base::string16 raw_separator =
      l10n_util::GetStringUTF16(IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR);
  return CreateAttributedString(raw_separator, DimTextColor());
}

@end
