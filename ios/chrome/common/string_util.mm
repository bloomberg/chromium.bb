// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/common/string_util.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"

namespace {
typedef BOOL (^ArrayFilterProcedure)(id object, NSUInteger index, BOOL* stop);
typedef NSString* (^SubstringExtractionProcedure)(NSUInteger);
}

NSString* ParseStringWithLink(NSString* text, NSRange* out_link_range) {
  // Find the range within |text| and create a substring without the link tags.
  NSRange begin_range = [text rangeOfString:@"BEGIN_LINK[ \t]*"
                                    options:NSRegularExpressionSearch];
  NSRange link_text_range = NSMakeRange(NSNotFound, 0);
  if (begin_range.length == 0) {
    if (out_link_range)
      *out_link_range = link_text_range;
    return text;
  }

  NSUInteger after_begin_link = NSMaxRange(begin_range);
  NSRange range_to_search_for_end_link =
      NSMakeRange(after_begin_link, text.length - after_begin_link);
  NSRange end_range = [text rangeOfString:@"[ \t]*END_LINK"
                                  options:NSRegularExpressionSearch
                                    range:range_to_search_for_end_link];
  if (end_range.length == 0) {
    if (out_link_range)
      *out_link_range = link_text_range;
    return text;
  }

  link_text_range.location = after_begin_link;
  link_text_range.length = end_range.location - link_text_range.location;
  base::scoped_nsobject<NSMutableString> out_text(
      [[NSMutableString alloc] init]);
  // First part - before the link.
  if (begin_range.location > 0)
    [out_text appendString:[text substringToIndex:begin_range.location]];

  // Link part.
  [out_text appendString:[text substringWithRange:link_text_range]];

  // Last part - after the link.
  NSUInteger after_end_link = NSMaxRange(end_range);
  if (after_end_link < [text length]) {
    [out_text appendString:[text substringFromIndex:after_end_link]];
  }

  link_text_range.location = begin_range.location;
  if (out_link_range)
    *out_link_range = link_text_range;
  return [NSString stringWithString:out_text];
}

// Ranges of unicode codepage containing drawing characters.
// 2190—21FF Arrows
// 2200—22FF Mathematical Operators
// 2300—23FF Miscellaneous Technical
// 2400—243F Control Pictures
// 2440—245F Optical Character Recognition
// 2460—24FF Enclosed Alphanumerics
// 2500—257F Box Drawing
// 2580—259F Block Elements
// 25A0—25FF Geometric Shapes
// 2600—26FF Miscellaneous Symbols
// 2700—27BF Dingbats
// 27C0—27EF Miscellaneous Mathematical Symbols-A
// 27F0—27FF Supplemental Arrows-A
// 2900—297F Supplemental Arrows-B
// 2980—29FF Miscellaneous Mathematical Symbols-B
// 2A00—2AFF Supplemental Mathematical Operators
// 2B00—2BFF Miscellaneous Symbols and Arrows
// The section 2800—28FF Braille Patterns must be preserved.
// The list of characters that must be deleted from the selection.
NSCharacterSet* GraphicCharactersSet() {
  static NSMutableCharacterSet* graphicalCharsSet;
  static dispatch_once_t dispatch_once_token;
  dispatch_once(&dispatch_once_token, ^{
    graphicalCharsSet = [[NSMutableCharacterSet alloc] init];
    NSRange graphicalCharsFirstRange = NSMakeRange(0x2190, 0x2800 - 0x2190);
    NSRange graphicalCharsSecondRange = NSMakeRange(0x2900, 0x2c00 - 0x2900);
    [graphicalCharsSet addCharactersInRange:graphicalCharsFirstRange];
    [graphicalCharsSet addCharactersInRange:graphicalCharsSecondRange];
  });
  return graphicalCharsSet;
}

NSString* CleanNSStringForDisplay(NSString* dirty, BOOL removeGraphicChars) {
  NSCharacterSet* wspace = [NSCharacterSet whitespaceAndNewlineCharacterSet];
  NSString* cleanString = dirty;
  if (removeGraphicChars) {
    cleanString = [[cleanString
        componentsSeparatedByCharactersInSet:GraphicCharactersSet()]
        componentsJoinedByString:@" "];
  }
  base::scoped_nsobject<NSMutableArray> spaceSeparatedCompoments(
      [[cleanString componentsSeparatedByCharactersInSet:wspace] mutableCopy]);
  ArrayFilterProcedure filter = ^(id object, NSUInteger index, BOOL* stop) {
    return [object isEqualToString:@""];
  };
  [spaceSeparatedCompoments
      removeObjectsAtIndexes:[spaceSeparatedCompoments
                                 indexesOfObjectsPassingTest:filter]];
  cleanString = [spaceSeparatedCompoments componentsJoinedByString:@" "];
  return cleanString;
}

std::string CleanStringForDisplay(const std::string& dirty,
                                  BOOL removeGraphicChars) {
  return base::SysNSStringToUTF8(CleanNSStringForDisplay(
      base::SysUTF8ToNSString(dirty), removeGraphicChars));
}

NSString* SubstringOfWidth(NSString* string,
                           NSDictionary* attributes,
                           CGFloat targetWidth,
                           BOOL trailing) {
  if (![string length])
    return nil;

  UIFont* font = [attributes objectForKey:NSFontAttributeName];
  DCHECK(font);

  // Function to get the correct substring while insulating against
  // length overrun/underrun.
  base::mac::ScopedBlock<SubstringExtractionProcedure> getSubstring;
  if (trailing) {
    getSubstring.reset([^NSString*(NSUInteger chars) {
      NSUInteger length = [string length];
      return [string substringFromIndex:length - MIN(length, chars)];
    } copy]);
  } else {
    getSubstring.reset([^NSString*(NSUInteger chars) {
      return [string substringToIndex:MIN(chars, [string length])];
    } copy]);
  }

  // Guess at the number of characters that will fit, assuming
  // the font's x-height is about 25% wider than an average character (25%
  // value was determined experimentally).
  NSUInteger characters =
      MIN(targetWidth / (font.xHeight * 0.8), [string length]);
  NSInteger increment = 1;
  NSString* substring = getSubstring.get()(characters);
  CGFloat prevWidth = [substring sizeWithAttributes:attributes].width;
  do {
    characters += increment;
    substring = getSubstring.get()(characters);
    CGFloat thisWidth = [substring sizeWithAttributes:attributes].width;
    if (prevWidth > targetWidth) {
      if (thisWidth <= targetWidth)
        break;  // Shrinking the string, found the right size.
      else
        increment = -1;  // Shrink the string
    } else if (prevWidth < targetWidth) {
      if (thisWidth < targetWidth)
        increment = 1;  // Grow the string
      else {
        substring = getSubstring.get()(characters - increment);
        break;  // Growing the string, found the right size.
      }
    }
    prevWidth = thisWidth;
  } while (characters > 0 && characters < [string length]);

  return substring;
}
