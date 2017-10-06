// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/navigation/placeholder_navigation_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace placeholder_navigation_util {

typedef PlatformTest PlaceholderNavigationUtilTest;

TEST_F(PlaceholderNavigationUtilTest, IsPlaceholderUrl) {
  // Valid placeholder URLs.
  EXPECT_TRUE(IsPlaceholderUrl(GURL("about:blank?for=")));
  EXPECT_TRUE(IsPlaceholderUrl(GURL("about:blank?for=chrome%3A%2F%2Fnewtab")));

  // Not an about:blank URL.
  EXPECT_FALSE(IsPlaceholderUrl(GURL::EmptyGURL()));
  // Missing ?for= query parameter.
  EXPECT_FALSE(IsPlaceholderUrl(GURL("about:blank")));
  EXPECT_FALSE(IsPlaceholderUrl(GURL("about:blank?chrome:%3A%2F%2Fnewtab")));
}

TEST_F(PlaceholderNavigationUtilTest, EncodReturnsEmptyOnInvalidUrls) {
  EXPECT_EQ(GURL::EmptyGURL(), CreatePlaceholderUrlForUrl(GURL::EmptyGURL()));
  EXPECT_EQ(GURL::EmptyGURL(), CreatePlaceholderUrlForUrl(GURL("notaurl")));
}

TEST_F(PlaceholderNavigationUtilTest, EncodeDecodeValidUrls) {
  {
    GURL original("chrome://chrome-urls");
    GURL encoded("about:blank?for=chrome%3A%2F%2Fchrome-urls");
    EXPECT_EQ(encoded, CreatePlaceholderUrlForUrl(original));
    EXPECT_EQ(original, ExtractUrlFromPlaceholderUrl(encoded));
  }
  {
    GURL original("about:blank");
    GURL encoded("about:blank?for=about%3Ablank");
    EXPECT_EQ(encoded, CreatePlaceholderUrlForUrl(original));
    EXPECT_EQ(original, ExtractUrlFromPlaceholderUrl(encoded));
  }
}

// Tests that invalid URLs will be rejected in decoding.
TEST_F(PlaceholderNavigationUtilTest, DecodeRejectInvalidUrls) {
  GURL encoded("about:blank?for=thisisnotanurl");
  EXPECT_EQ(GURL::EmptyGURL(), ExtractUrlFromPlaceholderUrl(encoded));
}

}  // namespace placeholder_navigation_util
}  // namespace web
