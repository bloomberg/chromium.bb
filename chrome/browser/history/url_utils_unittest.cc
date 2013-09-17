// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/url_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace history {

namespace {

TEST(UrlUtilsTest, CanonicalURLStringCompare) {
  // Comprehensive test by comparing each pair in sorted list. O(n^2).
  const char* sorted_list[] = {
    "http://www.gogle.com/redirects_to_google",
    "http://www.google.com",
    "http://www.google.com/",
    "http://www.google.com/?q",
    "http://www.google.com/A",
    "http://www.google.com/index.html",
    "http://www.google.com/test",
    "http://www.google.com/test?query",
    "http://www.google.com/test?r=3",
    "http://www.google.com/test#hash",
    "http://www.google.com/test/?query",
    "http://www.google.com/test/#hash",
    "http://www.google.com/test/zzzzz",
    "http://www.google.com/test$dollar",
    "http://www.google.com/test%E9%9B%80",
    "http://www.google.com/test-case",
    "http://www.google.com:80/",
    "https://www.google.com",
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(sorted_list); ++i) {
    EXPECT_FALSE(CanonicalURLStringCompare(sorted_list[i], sorted_list[i]))
        << " for \"" << sorted_list[i] << "\" < \"" << sorted_list[i] << "\"";
    // Every disjoint pair-wise comparison.
    for (size_t j = i + 1; j < ARRAYSIZE_UNSAFE(sorted_list); ++j) {
      EXPECT_TRUE(CanonicalURLStringCompare(sorted_list[i], sorted_list[j]))
          << " for \"" << sorted_list[i] << "\" < \"" << sorted_list[j] << "\"";
      EXPECT_FALSE(CanonicalURLStringCompare(sorted_list[j], sorted_list[i]))
          << " for \"" << sorted_list[j] << "\" < \"" << sorted_list[i] << "\"";
    }
  }
}

TEST(UrlUtilsTest, UrlIsPrefix) {
  struct {
    const char* s1;
    const char* s2;
  } true_cases[] = {
    {"http://www.google.com", "http://www.google.com"},
    {"http://www.google.com/a/b", "http://www.google.com/a/b"},
    {"http://www.google.com?test=3", "http://www.google.com/"},
    {"http://www.google.com/#hash", "http://www.google.com/?q"},
    {"http://www.google.com/", "http://www.google.com/test/with/dir/"},
    {"http://www.google.com:360", "http://www.google.com:360/?q=1234"},
    {"http://www.google.com:80", "http://www.google.com/gurl/is/smart"},
    {"http://www.google.com:80/", "http://www.google.com/"},
    {"http://www.google.com/test", "http://www.google.com/test/with/dir/"},
    {"http://www.google.com/test/", "http://www.google.com/test/with/dir"},
    {"http://www.google.com/test?", "http://www.google.com/test/with/dir/"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(true_cases); ++i) {
    EXPECT_TRUE(UrlIsPrefix(GURL(true_cases[i].s1), GURL(true_cases[i].s2)))
        << " for true_cases[" << i << "]";
  }
  struct {
    const char* s1;
    const char* s2;
  } false_cases[] = {
    {"http://www.google.com/test", "http://www.google.com"},
    {"http://www.google.com/a/b/", "http://www.google.com/a/b"},  // Arguable.
    {"http://www.google.co", "http://www.google.com"},
    {"http://google.com", "http://www.google.com"},
    {"http://www.google.com", "https://www.google.com"},
    {"http://www.google.com/path", "http://www.google.com:137/path"},
    {"http://www.google.com/same/dir", "http://www.youtube.com/same/dir"},
    {"http://www.google.com/te", "http://www.google.com/test"},
    {"http://www.google.com/test", "http://www.google.com/test-bed"},
    {"http://www.google.com/test-", "http://www.google.com/test?"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(false_cases); ++i) {
    EXPECT_FALSE(UrlIsPrefix(GURL(false_cases[i].s1), GURL(false_cases[i].s2)))
        << " for false_cases[" << i << "]";
  }
}

}  // namespace

}  // namespace history
