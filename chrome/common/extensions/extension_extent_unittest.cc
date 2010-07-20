// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension.h"

#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

static const int kAllSchemes =
    URLPattern::SCHEME_HTTP |
    URLPattern::SCHEME_HTTPS |
    URLPattern::SCHEME_FILE |
    URLPattern::SCHEME_FTP |
    URLPattern::SCHEME_CHROMEUI;

TEST(ExtensionExtentTest, Empty) {
  ExtensionExtent extent;
  EXPECT_FALSE(extent.ContainsURL(GURL("http://www.foo.com/bar")));
  EXPECT_FALSE(extent.ContainsURL(GURL()));
  EXPECT_FALSE(extent.ContainsURL(GURL("invalid")));
}

TEST(ExtensionExtentTest, One) {
  ExtensionExtent extent;
  extent.AddPattern(URLPattern(kAllSchemes, "http://www.google.com/*"));

  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/")));
  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/monkey")));
  EXPECT_FALSE(extent.ContainsURL(GURL("https://www.google.com/")));
  EXPECT_FALSE(extent.ContainsURL(GURL("https://www.microsoft.com/")));
}

TEST(ExtensionExtentTest, Two) {
  ExtensionExtent extent;
  extent.AddPattern(URLPattern(kAllSchemes, "http://www.google.com/*"));
  extent.AddPattern(URLPattern(kAllSchemes, "http://www.yahoo.com/*"));

  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/monkey")));
  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.yahoo.com/monkey")));
  EXPECT_FALSE(extent.ContainsURL(GURL("https://www.apple.com/monkey")));
}

TEST(ExtensionExtentTest, OverlapsWith) {
  ExtensionExtent extent1;
  extent1.AddPattern(URLPattern(kAllSchemes, "http://www.google.com/f*"));
  extent1.AddPattern(URLPattern(kAllSchemes, "http://www.yahoo.com/b*"));

  ExtensionExtent extent2;
  extent2.AddPattern(URLPattern(kAllSchemes, "http://www.reddit.com/f*"));
  extent2.AddPattern(URLPattern(kAllSchemes, "http://www.yahoo.com/z*"));

  ExtensionExtent extent3;
  extent3.AddPattern(URLPattern(kAllSchemes, "http://www.google.com/q/*"));
  extent3.AddPattern(URLPattern(kAllSchemes, "http://www.yahoo.com/b/*"));

  EXPECT_FALSE(extent1.OverlapsWith(extent2));
  EXPECT_FALSE(extent2.OverlapsWith(extent1));

  EXPECT_TRUE(extent1.OverlapsWith(extent3));
  EXPECT_TRUE(extent3.OverlapsWith(extent1));
}
