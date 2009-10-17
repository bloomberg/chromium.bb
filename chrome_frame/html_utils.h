// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines utility functions for working with html.

#ifndef CHROME_FRAME_HTML_UTILS_H_
#define CHROME_FRAME_HTML_UTILS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

// Forward declarations
class HtmlUtilUnittest;

//
// Class designed to take a string of HTML and extract from it named
// attribute values from named tags.
//
// Caveat: this class currently doesn't handle multi-word UTF-16 encoded
// characters. Doesn't handle implies that any data following such a
// character could possibly be misinterpreted.
//
class HTMLScanner {
 public:
  typedef std::wstring::const_iterator StrPos;

  // Structure maintaining const_iterators into html_string_.
  class StringRange {
    friend class HTMLScanner;
   public:
    StringRange();
    StringRange(StrPos start, StrPos end);

    bool LowerCaseEqualsASCII(const char* other) const;
    bool Equals(const wchar_t* other) const;

    // Copies the data described by StringRange into destination.
    std::wstring Copy() const;

    // If this StringRange represents a tag, this method extracts the name of
    // the tag and sticks it in tag_name.
    // Returns true if the tag name was successfully extracted.
    // Returns false if this string doesn't look like a valid tag.
    bool GetTagName(std::wstring* tag_name) const;

    // From a given string range, uses a string tokenizer to extract the value
    // of the named attribute if a simple scan finds that the attribute name is
    // present.
    //
    // Returns true if the named attribute can be located and it has a value
    // which has been placed in attribute_value.
    //
    // Note that the attribute value is unquoted here as well, so that
    // GetTagAttribute(*<foo bar="baz">*, L"bar", *out_value*) will stick
    // 'bar' in out_value and not '"bar"'.
    //
    // Returns false if the named attribute is not present in the tag or if it
    // did not have a value.
    //
    bool GetTagAttribute(const wchar_t* attribute_name,
                         StringRange* attribute_value) const;

    // Unquotes a StringRange by removing a matching pair of either ' or "
    // characters from the beginning and end of the string if present.
    // Returns true if string was modified, false otherwise.
    bool UnQuote();
   private:
     StrPos start_;
     StrPos end_;
  };

  typedef std::vector<StringRange> StringRangeList;

  // html_string must be a null-terminated string containing the HTML
  // to be scanned.
  explicit HTMLScanner(const wchar_t* html_string);

  // Returns the set of ranges denoting HTML tags that match the given name.
  // If stop_tag_name is given, then as soon as a tag with this name is
  // encountered this method will return.
  void GetTagsByName(const wchar_t* name, StringRangeList* tag_list,
                     const wchar_t* stop_tag_name);

 private:
  friend class HtmlUtilUnittest;
  FRIEND_TEST(HtmlUtilUnittest, BasicTest);

  // Given html_string which represents the remaining html range, this method
  // returns the next tag in tag and advances html_string to one character after
  // the end of tag. This method is intended to be called repeatedly to extract
  // all of the tags in sequence.
  //
  // Returns true if another tag was found and 'tag' was populated with a valid
  // range.
  // Returns false if we have reached the end of the html data.
  bool NextTag(StringRange* html_string, StringRange* tag);

  // Returns true if c can be found in quotes_, false otherwise
  bool IsQuote(wchar_t c);

  // Returns true if pos refers to the last character in an HTML comment in a
  // string described by html_string, false otherwise.
  // For example with html_string describing <!-- foo> -->, pos must refer to
  // the last > for this method to return true.
  bool IsHTMLCommentClose(StringRange* html_string, StrPos pos);

  // We store a (CollapsedWhitespace'd) copy of the html data.
  const std::wstring html_string_;

  // Store the string of quote characters to avoid repeated construction.
  const std::wstring quotes_;

  DISALLOW_COPY_AND_ASSIGN(HTMLScanner);
};

namespace http_utils {

// Adds "chromeframe/x.y" to the end of the User-Agent string (x.y is the
// version).  If the cf tag has already been added to the string,
// the original string is returned.
std::string AddChromeFrameToUserAgentValue(const std::string& value);

// Fetches the user agent from urlmon and adds chrome frame to the
// comment section.
// NOTE: The returned string includes the "User-Agent: " header name.
std::string GetDefaultUserAgentHeaderWithCFTag();

// Fetches the default user agent string from urlmon.
// This value does not include the "User-Agent:" header name.
std::string GetDefaultUserAgent();

// Returns the Chrome Frame user agent.  E.g. "chromeframe/1.0".
// Note that in unit tests this will be "chromeframe/0.0" due to the version
// table not being present in the unit test executable.
const char* GetChromeFrameUserAgent();

}  // namespace http_utils

#endif  // CHROME_FRAME_HTML_UTILS_H_
