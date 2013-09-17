// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites_cache.h"

#include <set>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

namespace {

class TopSitesCacheTest : public testing::Test {
 public:
  TopSitesCacheTest() {
  }

 protected:
  // Initializes |top_sites_| and |cache_| based on |spec|, which is a list of
  // URL strings with optional indents: indentated URLs redirect to the last
  // non-indented URL. Titles are assigned as "Title 1", "Title 2", etc., in the
  // order of appearance. See |kTopSitesSpecBasic| for an example.
  void InitTopSiteCache(const char** spec, int size);

  MostVisitedURLList top_sites_;
  TopSitesCache cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TopSitesCacheTest);
};

void TopSitesCacheTest::InitTopSiteCache(const char** spec, int size) {
  std::set<std::string> urls_seen;
  for (int i = 0; i < size; ++i) {
    const char* spec_item = spec[i];
    while (*spec_item && *spec_item == ' ')  // Eat indent.
      ++spec_item;
    if (urls_seen.find(spec_item) != urls_seen.end())
      NOTREACHED() << "Duplicate URL found: " << spec_item;
    urls_seen.insert(spec_item);
    if (spec_item == spec[i]) {  // No indent: add new MostVisitedURL.
      string16 title(ASCIIToUTF16("Title ") +
                     base::Uint64ToString16(top_sites_.size() + 1));
      top_sites_.push_back(MostVisitedURL(GURL(spec_item), title));
    }
    ASSERT_TRUE(!top_sites_.empty());
    // Set up redirect to canonical URL. Canonical URL redirects to itself, too.
    top_sites_.back().redirects.push_back(GURL(spec_item));
  }
  cache_.SetTopSites(top_sites_);
}

const char* kTopSitesSpecBasic[] = {
  "http://www.google.com",
  "  http://www.gogle.com",  // Redirects.
  "  http://www.gooogle.com",  // Redirects.
  "http://www.youtube.com/a/b",
  "  http://www.youtube.com/a/b?test=1",  // Redirects.
  "https://www.google.com/",
  "  https://www.gogle.com",  // Redirects.
  "http://www.example.com:3141/",
};

TEST_F(TopSitesCacheTest, GetCanonicalURL) {
  InitTopSiteCache(kTopSitesSpecBasic, arraysize(kTopSitesSpecBasic));
  struct {
    const char* expected;
    const char* query;
  } test_cases[] = {
    // Already is canonical: redirects.
    {"http://www.google.com/", "http://www.google.com"},
    // Exact match with stored URL: redirects.
    {"http://www.google.com/", "http://www.gooogle.com"},
    // Recognizes despite trailing "/": redirects
    {"http://www.google.com/", "http://www.gooogle.com/"},
    // Exact match with URL with query: redirects.
    {"http://www.youtube.com/a/b", "http://www.youtube.com/a/b?test=1"},
    // No match with URL with query: as-is.
    {"http://www.youtube.com/a/b?test", "http://www.youtube.com/a/b?test"},
    // Never-seen-before URL: as-is.
    {"http://maps.google.com/", "http://maps.google.com/"},
    // Changing port number, does not match: as-is.
    {"http://www.example.com:1234/", "http://www.example.com:1234"},
    // Smart enough to know that port 80 is HTTP: redirects.
    {"http://www.google.com/", "http://www.gooogle.com:80"},
    // Prefix should not work: as-is.
    {"http://www.youtube.com/a", "http://www.youtube.com/a"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    std::string expected(test_cases[i].expected);
    std::string query(test_cases[i].query);
    EXPECT_EQ(expected, cache_.GetCanonicalURL(GURL(query)).spec())
      << " for test_case[" << i << "]";
  }
}

TEST_F(TopSitesCacheTest, IsKnownUrl) {
  InitTopSiteCache(kTopSitesSpecBasic, arraysize(kTopSitesSpecBasic));
  // Matches.
  EXPECT_TRUE(cache_.IsKnownURL(GURL("http://www.google.com")));
  EXPECT_TRUE(cache_.IsKnownURL(GURL("http://www.gooogle.com")));
  EXPECT_TRUE(cache_.IsKnownURL(GURL("http://www.google.com/")));

  // Non-matches.
  EXPECT_FALSE(cache_.IsKnownURL(GURL("http://www.google.com?")));
  EXPECT_FALSE(cache_.IsKnownURL(GURL("http://www.google.net")));
  EXPECT_FALSE(cache_.IsKnownURL(GURL("http://www.google.com/stuff")));
  EXPECT_FALSE(cache_.IsKnownURL(GURL("https://www.gooogle.com")));
  EXPECT_FALSE(cache_.IsKnownURL(GURL("http://www.youtube.com/a")));
}

const char* kTopSitesSpecPrefix[] = {
  "http://www.google.com/",
  "  http://www.google.com/test?q=3",  // Redirects.
  "  http://www.google.com/test/y?b",  // Redirects.
  "http://www.google.com/2",
  "  http://www.google.com/test/q",  // Redirects.
  "  http://www.google.com/test/y?a",  // Redirects.
  "http://www.google.com/3",
  "  http://www.google.com/testing",  // Redirects.
  "http://www.google.com/test-hyphen",
  "http://www.google.com/sh",
  "  http://www.google.com/sh/1/2",  // Redirects.
  "http://www.google.com/sh/1",
};

TEST_F(TopSitesCacheTest, GetCanonicalURLForPrefix) {
  InitTopSiteCache(kTopSitesSpecPrefix, arraysize(kTopSitesSpecPrefix));
  struct {
    const char* expected;
    const char* query;
  } test_cases[] = {
    // Already is canonical: redirects.
    {"http://www.google.com/", "http://www.google.com"},
    // Exact match with stored URL: redirects.
    {"http://www.google.com/", "http://www.google.com/test?q=3"},
    // Prefix match: redirects.
    {"http://www.google.com/", "http://www.google.com/test"},
    // Competing prefix match: redirects to closest.
    {"http://www.google.com/2", "http://www.google.com/test/q"},
    // Multiple prefix matches: redirects to first.
    {"http://www.google.com/2", "http://www.google.com/test/y"},
    // No prefix match: as-is.
    {"http://www.google.com/no-match", "http://www.google.com/no-match"},
    // String prefix match but not URL-prefix match: as-is.
    {"http://www.google.com/t", "http://www.google.com/t"},
    // Different protocol: as-is.
    {"https://www.google.com/test", "https://www.google.com/test"},
    // Smart enough to know that port 80 is HTTP: redirects.
    {"http://www.google.com/", "http://www.google.com:80/test"},
    // Exact match, unaffected by "http://www.google.com/sh/1": redirects.
    {"http://www.google.com/sh", "http://www.google.com/sh/1/2"},
    // Suffix match only: as-is
    {"http://www.google.com/sh/1/2/3", "http://www.google.com/sh/1/2/3"},
    // Exact match, unaffected by "http://www.google.com/sh": redirects.
    {"http://www.google.com/sh/1", "http://www.google.com/sh/1"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    std::string expected(test_cases[i].expected);
    std::string query(test_cases[i].query);
    EXPECT_EQ(expected, cache_.GetCanonicalURLForPrefix(GURL(query)).spec())
      << " for test_case[" << i << "]";
  }
}

}  // namespace

}  // namespace history
