// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_STRING_UTIL_H_
#define IOS_CHROME_COMMON_STRING_UTIL_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#include <string>

// Parses a string with an embedded link inside, delineated by BEGIN_LINK and
// END_LINK. Returns the string without the link delimiters. If |out_link_range|
// is not null, then it is filled out with the range of the link in the returned
// string.
// If no link is found, then it returns |text| and sets |out_link_range| to
// {NSNotFound, 0}.
NSString* ParseStringWithLink(NSString* text, NSRange* out_link_range);

// Utility method that returns an NSCharacterSet containing Unicode graphics
// and drawing characters (but not including the Braille Patterns characters).
NSCharacterSet* GraphicCharactersSet();

// Cleans an NSString by collapsing whitespace and removing leading and trailing
// spaces. If |removeGraphicChars| is true, unicode graphic characters will also
// be removed from the string.
NSString* CleanNSStringForDisplay(NSString* dirty, BOOL removeGraphicChars);

// Cleans a std::string identically to CleanNSStringForDisplay()
std::string CleanStringForDisplay(const std::string& dirty,
                                  BOOL removeGraphicChars);

// Find the longest leading substring of |string| that, when rendered with
// |attributes|, will fit on a single line inside |targetWidth|. If |trailing|
// is YES, then find the trailing (instead of leading) substring.
NSString* SubstringOfWidth(NSString* string,
                           NSDictionary* attributes,
                           CGFloat targetWidth,
                           BOOL trailing);

#endif  // IOS_CHROME_COMMON_STRING_UTIL_H_
