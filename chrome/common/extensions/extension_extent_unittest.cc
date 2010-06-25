// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension.h"

#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ExtensionExtentTest, Empty) {
  ExtensionExtent extent;
  EXPECT_FALSE(extent.ContainsURL(GURL("http://www.foo.com/bar")));
  EXPECT_FALSE(extent.ContainsURL(GURL()));
  EXPECT_FALSE(extent.ContainsURL(GURL("invalid")));
}

TEST(ExtensionExtentTest, One) {
  ExtensionExtent extent;
  extent.AddPattern(URLPattern("http://www.google.com/*"));

  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/")));
  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/monkey")));
  EXPECT_FALSE(extent.ContainsURL(GURL("https://www.google.com/")));
  EXPECT_FALSE(extent.ContainsURL(GURL("https://www.microsoft.com/")));
}

TEST(ExtensionExtentTest, Two) {
  ExtensionExtent extent;
  extent.AddPattern(URLPattern("http://www.google.com/*"));
  extent.AddPattern(URLPattern("http://www.yahoo.com/*"));

  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/monkey")));
  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.yahoo.com/monkey")));
  EXPECT_FALSE(extent.ContainsURL(GURL("https://www.apple.com/monkey")));
}
