// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/google/google_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using google_util::IsGoogleHomePageUrl;

TEST(GoogleUtilTest, GoodHomePagesNonSecure) {
  // Valid home page hosts.
  EXPECT_TRUE(IsGoogleHomePageUrl(GoogleURLTracker::kDefaultGoogleHomepage));
  EXPECT_TRUE(IsGoogleHomePageUrl("http://google.com"));
  EXPECT_TRUE(IsGoogleHomePageUrl("http://www.google.com"));
  EXPECT_TRUE(IsGoogleHomePageUrl("http://www.google.ca"));
  EXPECT_TRUE(IsGoogleHomePageUrl("http://www.google.co.uk"));
  EXPECT_TRUE(IsGoogleHomePageUrl("http://www.google.com:80/"));

  // Only the paths /, /webhp, and /ig.* are valid.  Query parameters are
  // ignored.
  EXPECT_TRUE(IsGoogleHomePageUrl("http://www.google.com/"));
  EXPECT_TRUE(IsGoogleHomePageUrl("http://www.google.com/webhp"));
  EXPECT_TRUE(IsGoogleHomePageUrl("http://www.google.com/webhp?rlz=TEST"));
  EXPECT_TRUE(IsGoogleHomePageUrl("http://www.google.com/ig"));
  EXPECT_TRUE(IsGoogleHomePageUrl("http://www.google.com/ig/foo"));
  EXPECT_TRUE(IsGoogleHomePageUrl("http://www.google.com/ig?rlz=TEST"));
  EXPECT_TRUE(IsGoogleHomePageUrl("http://www.google.com/ig/foo?rlz=TEST"));
}

TEST(GoogleUtilTest, GoodHomePagesSecure) {
  // Valid home page hosts.
  EXPECT_TRUE(IsGoogleHomePageUrl("https://google.com"));
  EXPECT_TRUE(IsGoogleHomePageUrl("https://www.google.com"));
  EXPECT_TRUE(IsGoogleHomePageUrl("https://www.google.ca"));
  EXPECT_TRUE(IsGoogleHomePageUrl("https://www.google.co.uk"));
  EXPECT_TRUE(IsGoogleHomePageUrl("https://www.google.com:443/"));

  // Only the paths /, /webhp, and /ig.* are valid.  Query parameters are
  // ignored.
  EXPECT_TRUE(IsGoogleHomePageUrl("https://www.google.com/"));
  EXPECT_TRUE(IsGoogleHomePageUrl("https://www.google.com/webhp"));
  EXPECT_TRUE(IsGoogleHomePageUrl("https://www.google.com/webhp?rlz=TEST"));
  EXPECT_TRUE(IsGoogleHomePageUrl("https://www.google.com/ig"));
  EXPECT_TRUE(IsGoogleHomePageUrl("https://www.google.com/ig/foo"));
  EXPECT_TRUE(IsGoogleHomePageUrl("https://www.google.com/ig?rlz=TEST"));
  EXPECT_TRUE(IsGoogleHomePageUrl("https://www.google.com/ig/foo?rlz=TEST"));
}

TEST(GoogleUtilTest, BadHomePages) {
  EXPECT_FALSE(IsGoogleHomePageUrl(""));

  // If specified, only the "www" subdomain is OK.
  EXPECT_FALSE(IsGoogleHomePageUrl("http://maps.google.com"));
  EXPECT_FALSE(IsGoogleHomePageUrl("http://foo.google.com"));

  // No non-standard port numbers.
  EXPECT_FALSE(IsGoogleHomePageUrl("http://www.google.com:1234"));
  EXPECT_FALSE(IsGoogleHomePageUrl("https://www.google.com:5678"));

  // Invalid TLDs.
  EXPECT_FALSE(IsGoogleHomePageUrl("http://www.google.abc"));
  EXPECT_FALSE(IsGoogleHomePageUrl("http://www.google.com.abc"));
  EXPECT_FALSE(IsGoogleHomePageUrl("http://www.google.abc.com"));
  EXPECT_FALSE(IsGoogleHomePageUrl("http://www.google.ab.cd"));
  EXPECT_FALSE(IsGoogleHomePageUrl("http://www.google.uk.qq"));

  // Must be http or https.
  EXPECT_FALSE(IsGoogleHomePageUrl("ftp://www.google.com"));
  EXPECT_FALSE(IsGoogleHomePageUrl("file://does/not/exist"));
  EXPECT_FALSE(IsGoogleHomePageUrl("bad://www.google.com"));
  EXPECT_FALSE(IsGoogleHomePageUrl("www.google.com"));

  // Only the paths /, /webhp, and /ig.* are valid.
  EXPECT_FALSE(IsGoogleHomePageUrl("http://www.google.com/abc"));
  EXPECT_FALSE(IsGoogleHomePageUrl("http://www.google.com/webhpabc"));
  EXPECT_FALSE(IsGoogleHomePageUrl("http://www.google.com/webhp/abc"));
  EXPECT_FALSE(IsGoogleHomePageUrl("http://www.google.com/abcig"));
  EXPECT_FALSE(IsGoogleHomePageUrl("http://www.google.com/webhp/ig"));
}
