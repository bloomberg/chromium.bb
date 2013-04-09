// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

using google_util::IsGoogleDomainUrl;
using google_util::IsGoogleHomePageUrl;
using google_util::IsGoogleSearchUrl;

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

  // A search URL should not be identified as a home page URL.
  EXPECT_FALSE(IsGoogleHomePageUrl("http://www.google.com/search?q=something"));

  // Path is case sensitive.
  EXPECT_FALSE(IsGoogleHomePageUrl("https://www.google.com/WEBHP"));
}

TEST(GoogleUtilTest, GoodSearchPagesNonSecure) {
  // Queries with path "/search" need to have the query parameter in either
  // the url parameter or the hash fragment.
  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.com/search?q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.com/search#q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.com/search?name=bob&q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.com/search?name=bob#q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.com/search?name=bob#age=24&q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.co.uk/search?q=something"));
  // It's actually valid for both to have the query parameter.
  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.com/search?q=something#q=other"));

  // Queries with path "/webhp", "/" or "" need to have the query parameter in
  // the hash fragment.
  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.com/webhp#q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.com/webhp#name=bob&q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.com/webhp?name=bob#q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.com/webhp?name=bob#age=24&q=something"));

  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.com/#q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.com/#name=bob&q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.com/?name=bob#q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.com/?name=bob#age=24&q=something"));

  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.com#q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.com#name=bob&q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.com?name=bob#q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "http://www.google.com?name=bob#age=24&q=something"));
}

TEST(GoogleUtilTest, GoodSearchPagesSecure) {
  // Queries with path "/search" need to have the query parameter in either
  // the url parameter or the hash fragment.
  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.com/search?q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.com/search#q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.com/search?name=bob&q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.com/search?name=bob#q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.com/search?name=bob#age=24&q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.co.uk/search?q=something"));
  // It's actually valid for both to have the query parameter.
  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.com/search?q=something#q=other"));

  // Queries with path "/webhp", "/" or "" need to have the query parameter in
  // the hash fragment.
  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.com/webhp#q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.com/webhp#name=bob&q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.com/webhp?name=bob#q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.com/webhp?name=bob#age=24&q=something"));

  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.com/#q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.com/#name=bob&q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.com/?name=bob#q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.com/?name=bob#age=24&q=something"));

  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.com#q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.com#name=bob&q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.com?name=bob#q=something"));
  EXPECT_TRUE(IsGoogleSearchUrl(
      "https://www.google.com?name=bob#age=24&q=something"));
}

TEST(GoogleUtilTest, BadSearches) {
  // A home page URL should not be identified as a search URL.
  EXPECT_FALSE(IsGoogleSearchUrl(GoogleURLTracker::kDefaultGoogleHomepage));
  EXPECT_FALSE(IsGoogleSearchUrl("http://google.com"));
  EXPECT_FALSE(IsGoogleSearchUrl("http://www.google.com"));
  EXPECT_FALSE(IsGoogleSearchUrl("http://www.google.com/search"));
  EXPECT_FALSE(IsGoogleSearchUrl("http://www.google.com/search?"));

  // Must be http or https
  EXPECT_FALSE(IsGoogleSearchUrl(
      "ftp://www.google.com/search?q=something"));
  EXPECT_FALSE(IsGoogleSearchUrl(
      "file://does/not/exist/search?q=something"));
  EXPECT_FALSE(IsGoogleSearchUrl(
      "bad://www.google.com/search?q=something"));
  EXPECT_FALSE(IsGoogleSearchUrl(
      "www.google.com/search?q=something"));

  // Can't have an empty query parameter.
  EXPECT_FALSE(IsGoogleSearchUrl(
      "http://www.google.com/search?q="));
  EXPECT_FALSE(IsGoogleSearchUrl(
      "http://www.google.com/search?name=bob&q="));
  EXPECT_FALSE(IsGoogleSearchUrl(
      "http://www.google.com/webhp#q="));
  EXPECT_FALSE(IsGoogleSearchUrl(
      "http://www.google.com/webhp#name=bob&q="));

  // Home page searches without a hash fragment query parameter are invalid.
  EXPECT_FALSE(IsGoogleSearchUrl(
      "http://www.google.com/webhp?q=something"));
  EXPECT_FALSE(IsGoogleSearchUrl(
      "http://www.google.com/webhp?q=something#no=good"));
  EXPECT_FALSE(IsGoogleSearchUrl(
      "http://www.google.com/webhp?name=bob&q=something"));
  EXPECT_FALSE(IsGoogleSearchUrl(
      "http://www.google.com/?q=something"));
  EXPECT_FALSE(IsGoogleSearchUrl(
      "http://www.google.com?q=something"));

  // Some paths are outright invalid as searches.
  EXPECT_FALSE(IsGoogleSearchUrl(
      "http://www.google.com/notreal?q=something"));
  EXPECT_FALSE(IsGoogleSearchUrl(
      "http://www.google.com/chrome?q=something"));
  EXPECT_FALSE(IsGoogleSearchUrl(
      "http://www.google.com/search/nogood?q=something"));
  EXPECT_FALSE(IsGoogleSearchUrl(
      "http://www.google.com/webhp/nogood#q=something"));
  EXPECT_FALSE(IsGoogleSearchUrl(""));

  // Case sensitive paths.
  EXPECT_FALSE(IsGoogleSearchUrl(
      "http://www.google.com/SEARCH?q=something"));
  EXPECT_FALSE(IsGoogleSearchUrl(
      "http://www.google.com/WEBHP#q=something"));
}

TEST(GoogleUtilTest, GoogleDomains) {
  // Test some good Google domains (valid TLDs).
  EXPECT_TRUE(IsGoogleDomainUrl("http://www.google.com",
                                google_util::ALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl("http://google.com",
                                google_util::ALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl("http://www.google.ca",
                                google_util::ALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl("http://www.google.biz.tj",
                                google_util::ALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl("http://www.google.com/search?q=something",
                                google_util::ALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl("http://www.google.com/webhp",
                                google_util::ALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));

  // Test some bad Google domains (invalid TLDs).
  EXPECT_FALSE(IsGoogleDomainUrl("http://www.google.notrealtld",
                                 google_util::ALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl("http://www.google.faketld/search?q=something",
                                 google_util::ALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl("http://www.yahoo.com",
                                 google_util::ALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));

  // Test subdomain checks.
  EXPECT_TRUE(IsGoogleDomainUrl("http://images.google.com",
                                google_util::ALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl("http://images.google.com",
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl("http://google.com",
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl("http://www.google.com",
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));

  // Port and scheme checks.
  EXPECT_TRUE(IsGoogleDomainUrl("http://www.google.com:80",
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl("http://www.google.com:123",
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl("https://www.google.com:443",
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl("http://www.google.com:123",
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl("http://www.google.com:123",
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::ALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl("https://www.google.com:123",
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::ALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl("http://www.google.com:80",
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::ALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl("https://www.google.com:443",
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::ALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl("file://www.google.com",
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl("doesnotexist://www.google.com",
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));

  // Test overriding with --instant-url works.
  EXPECT_FALSE(IsGoogleDomainUrl("http://test.foo.com",
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl("http://test.foo.com:1234",
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kInstantURL, "http://test.foo.com:1234/bar");
  EXPECT_FALSE(IsGoogleDomainUrl("http://test.foo.com",
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl("http://test.foo.com:1234",
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_FALSE(IsGoogleDomainUrl("file://test.foo.com:1234",
                                 google_util::DISALLOW_SUBDOMAIN,
                                 google_util::DISALLOW_NON_STANDARD_PORTS));
  EXPECT_TRUE(IsGoogleDomainUrl("http://www.google.com",
                                google_util::DISALLOW_SUBDOMAIN,
                                google_util::DISALLOW_NON_STANDARD_PORTS));
}
