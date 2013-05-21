// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_api_util.h"

#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {
namespace drive {
namespace util {

TEST(DriveApiUtilTest, EscapeQueryStringValue) {
  EXPECT_EQ("abcde", EscapeQueryStringValue("abcde"));
  EXPECT_EQ("\\'", EscapeQueryStringValue("'"));
  EXPECT_EQ("\\'abcde\\'", EscapeQueryStringValue("'abcde'"));
  EXPECT_EQ("\\\\", EscapeQueryStringValue("\\"));
  EXPECT_EQ("\\\\\\'", EscapeQueryStringValue("\\'"));
}

TEST(DriveApiUtilTest, TranslateQuery) {
  EXPECT_EQ("", TranslateQuery(""));
  EXPECT_EQ("fullText contains 'dog'", TranslateQuery("dog"));
  EXPECT_EQ("fullText contains 'dog' and fullText contains 'cat'",
            TranslateQuery("dog cat"));
  EXPECT_EQ("not fullText contains 'cat'", TranslateQuery("-cat"));
  EXPECT_EQ("fullText contains 'dog cat'", TranslateQuery("\"dog cat\""));

  // Should handles full-width white space correctly.
  // Note: \xE3\x80\x80 (\u3000) is Ideographic Space (a.k.a. Japanese
  //   full-width whitespace).
  EXPECT_EQ("fullText contains 'dog' and fullText contains 'cat'",
            TranslateQuery("dog" "\xE3\x80\x80" "cat"));

  // If the quoted token is not closed (i.e. the last '"' is missing),
  // we handle the remaining string is one token, as a fallback.
  EXPECT_EQ("fullText contains 'dog cat'", TranslateQuery("\"dog cat"));

  // For quoted text with leading '-'.
  EXPECT_EQ("not fullText contains 'dog cat'", TranslateQuery("-\"dog cat\""));

  // Empty tokens should be simply ignored.
  EXPECT_EQ("", TranslateQuery("-"));
  EXPECT_EQ("", TranslateQuery("\"\""));
  EXPECT_EQ("", TranslateQuery("-\"\""));
  EXPECT_EQ("", TranslateQuery("\"\"\"\""));
  EXPECT_EQ("", TranslateQuery("\"\" \"\""));
  EXPECT_EQ("fullText contains 'dog'", TranslateQuery("\"\" dog \"\""));
}

TEST(FileSystemUtilTest, ExtractResourceIdFromUrl) {
  EXPECT_EQ("file:2_file_resource_id", ExtractResourceIdFromUrl(
      GURL("https://file1_link_self/file:2_file_resource_id")));
  // %3A should be unescaped.
  EXPECT_EQ("file:2_file_resource_id", ExtractResourceIdFromUrl(
      GURL("https://file1_link_self/file%3A2_file_resource_id")));

  // The resource ID cannot be extracted, hence empty.
  EXPECT_EQ("", ExtractResourceIdFromUrl(GURL("https://www.example.com/")));
}

}  // namespace util
}  // namespace drive
}  // namespace google_apis
