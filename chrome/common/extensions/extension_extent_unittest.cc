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

TEST(ExtensionExtentTest, OriginOnly) {
  ExtensionExtent extent;
  extent.set_origin(GURL("http://www.google.com/"));

  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/")));
  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/foo")));
  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/foobar")));
  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/foo/bar")));
  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/?stuff#here")));

  EXPECT_FALSE(extent.ContainsURL(GURL("https://www.google.com/")));
  EXPECT_FALSE(extent.ContainsURL(GURL("http://www.google.com:8080/")));
}

TEST(ExtensionExtentTest, OriginAndOnePath) {
  ExtensionExtent extent;
  extent.set_origin(GURL("http://www.google.com/"));
  extent.add_path("foo");

  EXPECT_FALSE(extent.ContainsURL(GURL("http://www.google.com/")));
  EXPECT_FALSE(extent.ContainsURL(GURL("http://www.google.com/fo")));

  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/foo")));
  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/FOO")));
  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/foobar")));
  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/foo/bar")));
}

TEST(ExtensionExtentTest, OriginAndTwoPaths) {
  ExtensionExtent extent;
  extent.set_origin(GURL("http://www.google.com/"));
  extent.add_path("foo");
  extent.add_path("hot");

  EXPECT_FALSE(extent.ContainsURL(GURL("http://www.google.com/monkey")));

  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/foo")));
  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/hot")));
  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/foobar")));
  EXPECT_TRUE(extent.ContainsURL(GURL("http://www.google.com/hotdog")));
}
