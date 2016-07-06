// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/search/search_urls.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace search {
typedef testing::Test SearchURLsTest;

TEST_F(SearchURLsTest, MatchesOriginAndPath) {
  EXPECT_TRUE(MatchesOriginAndPath(
      GURL("http://example.com/path"),
      GURL("http://example.com/path?param")));
  EXPECT_FALSE(MatchesOriginAndPath(
      GURL("http://not.example.com/path"),
      GURL("http://example.com/path")));
  EXPECT_TRUE(MatchesOriginAndPath(
      GURL("http://example.com:80/path"),
      GURL("http://example.com/path")));
  EXPECT_FALSE(MatchesOriginAndPath(
      GURL("http://example.com:8080/path"),
      GURL("http://example.com/path")));
  EXPECT_FALSE(MatchesOriginAndPath(
      GURL("ftp://example.com/path"),
      GURL("http://example.com/path")));
  EXPECT_FALSE(MatchesOriginAndPath(
      GURL("http://example.com/path"),
      GURL("https://example.com/path")));
  EXPECT_TRUE(MatchesOriginAndPath(
      GURL("https://example.com/path"),
      GURL("http://example.com/path")));
  EXPECT_FALSE(MatchesOriginAndPath(
      GURL("http://example.com/path"),
      GURL("http://example.com/another-path")));
}

}  // namespace search
