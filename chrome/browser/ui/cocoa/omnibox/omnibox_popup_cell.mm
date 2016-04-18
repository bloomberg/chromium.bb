// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/omnibox/omnibox_popup_cell.h"

#include <stddef.h>

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
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"

namespace {

// How far to offset image column from the left.
const CGFloat kImageXOffset = 5.0;

// How far to offset image and text.
const CGFloat kPaddingOffset = 3.0;

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
  return skia::SkColorToCalibratedNSColor(SkColorSetRGB(0x3d, 0x94, 0x00));
}
NSColor* NegativeTextColor() {
  return skia::SkColorToCalibratedNSColor(SkColorSetRGB(0xdd, 0x4b, 0x39));
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

NSAttributedString* CreateAnswerStringHelper(const base::string16& text,
                                             NSInteger style_type,
                                             bool is_bold) {
  NSDictionary* answer_style = nil;
  switch (style_type) {
    case SuggestionAnswer::TOP_ALIGNED:
      answer_style = @{
        NSForegroundColorAttributeName : DimTextColor(),
        NSFontAttributeName : LargeSuperscriptFont(),
        NSSuperscriptAttributeName : @1
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
    case SuggestionAnswer::PERSONALIZED_SUGGESTION:
      answer_style = @{
        NSForegroundColorAttributeName : ContentTextColor(),
        NSFontAttributeName : FieldFont()
      };
      break;
    case SuggestionAnswer::ANSWER_TEXT_MEDIUM:
      answer_style = @{
        NSForegroundColorAttributeName : ContentTextColor(),
        NSFontAttributeName : FieldFont()
      };
      break;
    case SuggestionAnswer::ANSWER_TEXT_LARGE:
      answer_style = @{
        NSForegroundColorAttributeName : ContentTextColor(),
        NSFontAttributeName : LargeFont()
      };
      break;
    case SuggestionAnswer::SUGGESTION_SECONDARY_TEXT_SMALL:
      answer_style = @{
        NSForegroundColorAttributeName : DimTextColor(),
        NSFontAttributeName : SmallFont()
      };
      break;
    case SuggestionAnswer::SUGGESTION_SECONDARY_TEXT_MEDIUM:
      answer_style = @{
        NSForegroundColorAttributeName : DimTextColor(),
        NSFontAttributeName : FieldFont()
      };
      break;
    case SuggestionAnswer::SUGGESTION:  // Fall through.
    default:
      answer_style = @{
        NSForegroundColorAttributeName : ContentTextColor (),
        NSFontAttributeName : FieldFont()
      };
      break;
  }

  if (is_bold) {
    NSMutableDictionary* bold_style = [answer_style mutableCopy];
    // TODO(dschuyler): Account for bolding fonts other than FieldFont.
    // Field font is the only one currently necessary to bold.
    [bold_style setObject:BoldFieldFont() forKey:NSFontAttributeName];
    answer_style = bold_style;
  }

  return [[[NSAttributedString alloc]
      initWithString:base::SysUTF16ToNSString(text)
          attributes:answer_style] autorelease];
}

NSAttributedString* CreateAnswerString(const base::string16& text,
                                       NSInteger style_type) {
  // TODO(dschuyler): make this better.  Right now this only supports unnested
  // bold tags.  In the future we'll need to flag unexpected tags while adding
  // support for b, i, u, sub, and sup.  We'll also need to support HTML
  // entities (&lt; for '<', etc.).
  const base::string16 begin_tag = base::ASCIIToUTF16("<b>");
  const base::string16 end_tag = base::ASCIIToUTF16("</b>");
  size_t begin = 0;
  base::scoped_nsobject<NSMutableAttributedString> result(
      [[NSMutableAttributedString alloc] init]);
  while (true) {
    size_t end = text.find(begin_tag, begin);
    if (end == base::string16::npos) {
      [result
          appendAttributedString:CreateAnswerStringHelper(
                                         text.substr(begin),
                                         style_type, false)];
      break;
    }
    [result appendAttributedString:CreateAnswerStringHelper(
                                       text.substr(begin, end - begin),
                                       style_type, false)];
    begin = end + begin_tag.length();
    end = text.find(end_tag, begin);
    if (end == base::string16::npos)
      break;
    [result appendAttributedString:CreateAnswerStringHelper(
                                       text.substr(begin, end - begin),
                                       style_type, true)];
    begin = end + end_tag.length();
  }
  return result.autorelease();
}

NSAttributedString* CreateAnswerLine(const SuggestionAnswer::ImageLine& line) {
  base::scoped_nsobject<NSMutableAttributedString> answer_string(
      [[NSMutableAttributedString alloc] init]);
  DCHECK(!line.text_fields().empty());
  for (const SuggestionAnswer::TextField& text_field : line.text_fields()) {
    [answer_string
        appendAttributedString:CreateAnswerString(text_field.text(),
                                                  text_field.type())];
  }
  const base::string16 space(base::ASCIIToUTF16(" "));
  const SuggestionAnswer::TextField* text_field = line.additional_text();
  if (text_field) {
    [answer_string
        appendAttributedString:CreateAnswerString(space + text_field->text(),
                                                  text_field->type())];
  }
  text_field = line.status_text();
  if (text_field) {
    [answer_string
        appendAttributedString:CreateAnswerString(space + text_field->text(),
                                                  text_field->type())];
  }
  base::scoped_nsobject<NSMutableParagraphStyle> style(
      [[NSMutableParagraphStyle alloc] init]);
  [style setLineBreakMode:NSLineBreakByTruncatingTail];
  [style setTighteningFactorForTruncation:0.0];
  [answer_string addAttribute:NSParagraphStyleAttributeName
                            value:style
                            range:NSMakeRange(0, [answer_string length])];
  return answer_string.autorelease();
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
                  origin:(NSPoint)origin
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
@synthesize isAnswer = isAnswer_;
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

    isAnswer_ = !!match.answer;
    if (isAnswer_) {
      contents_ = [CreateAnswerLine(match.answer->first_line()) retain];
      description_ = [CreateAnswerLine(match.answer->second_line()) retain];
    } else {
      contents_ = [CreateClassifiedAttributedString(
          match.contents, ContentTextColor(), match.contents_class) retain];
      if (!match.description.empty()) {
        description_ = [CreateClassifiedAttributedString(
            match.description, DimTextColor(), match.description_class) retain];
      }
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
      [cellData isAnswer],
      !AutocompleteMatch::IsSearchType([cellData matchType]),
      &contentsMaxWidth,
      &descriptionMaxWidth);

  NSRect imageRect = cellFrame;
  imageRect.size = [[cellData image] size];
  imageRect.origin.x += kImageXOffset;
  imageRect.origin.y += kPaddingOffset;
  [[cellData image] drawInRect:FlipIfRTL(imageRect, cellFrame)
                      fromRect:NSZeroRect
                     operation:NSCompositeSourceOver
                      fraction:1.0
                respectFlipped:YES
                         hints:nil];

  NSPoint origin = NSMakePoint(kTextStartOffset, kPaddingOffset);
  if ([cellData matchType] == AutocompleteMatchType::SEARCH_SUGGEST_TAIL) {
    // Infinite suggestions are rendered with a prefix (usually ellipsis), which
    // appear vertically stacked.
    origin.x += [self drawMatchPrefixWithFrame:cellFrame
                                     tableView:tableView
                          withContentsMaxWidth:&contentsMaxWidth];
  }
  origin.x += [self drawMatchPart:[cellData contents]
                        withFrame:cellFrame
                           origin:origin
                     withMaxWidth:contentsMaxWidth];

  if (descriptionMaxWidth > 0) {
    if ([cellData isAnswer]) {
      origin =
          NSMakePoint(kTextStartOffset, kContentLineHeight - kPaddingOffset);
      CGFloat imageSize = [tableView answerLineHeight];
      NSRect imageRect =
          NSMakeRect(NSMinX(cellFrame) + origin.x, NSMinY(cellFrame) + origin.y,
                     imageSize, imageSize);
      [[cellData answerImage] drawInRect:FlipIfRTL(imageRect, cellFrame)
                                fromRect:NSZeroRect
                               operation:NSCompositeSourceOver
                                fraction:1.0
                          respectFlipped:YES
                                   hints:nil];
      if ([cellData answerImage])
        origin.x += imageSize + kPaddingOffset;
    } else {
      origin.x += [self drawMatchPart:[tableView separator]
                            withFrame:cellFrame
                               origin:origin
                         withMaxWidth:separatorWidth];
    }
    origin.x += [self drawMatchPart:[cellData description]
                          withFrame:cellFrame
                             origin:origin
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
               origin:NSMakePoint(prefixOffset + kTextStartOffset, 0)
         withMaxWidth:prefixWidth];
  return offset;
}

- (CGFloat)drawMatchPart:(NSAttributedString*)attributedString
               withFrame:(NSRect)cellFrame
                  origin:(NSPoint)origin
            withMaxWidth:(int)maxWidth {
  NSRect renderRect = NSIntersectionRect(
      cellFrame, NSOffsetRect(cellFrame, origin.x, origin.y));
  renderRect.size.width =
      std::min(NSWidth(renderRect), static_cast<CGFloat>(maxWidth));
  if (!NSIsEmptyRect(renderRect))
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
  if (!base::StartsWith(match.fill_into_edit, inputText,
                        base::CompareCase::SENSITIVE) ||
      !base::EndsWith(match.fill_into_edit, match.contents,
                      base::CompareCase::SENSITIVE) ||
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
