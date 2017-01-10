// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/origin_filter_builder.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace url {

namespace {

struct TestCase {
  std::string url;
  bool should_match;
};

void RunTestCase(
    TestCase test_case, const base::Callback<bool(const GURL&)>& filter) {
  if (test_case.should_match)
    EXPECT_TRUE(filter.Run(GURL(test_case.url)));
  else
    EXPECT_FALSE(filter.Run(GURL(test_case.url)));
}

}  // namespace

TEST(OriginFilterBuilderTest, Noop) {
  // An no-op filter matches everything.
  base::Callback<bool(const GURL&)> filter =
      OriginFilterBuilder::BuildNoopFilter();

  TestCase test_cases[] = {
      { "https://www.google.com", true },
      { "https://www.chrome.com", true },
      { "invalid url spec", true }
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(OriginFilterBuilderTest, Whitelist) {
  OriginFilterBuilder builder(OriginFilterBuilder::WHITELIST);
  builder.AddOrigin(Origin(GURL("https://www.google.com")));
  builder.AddOrigin(Origin(GURL("http://www.example.com")));
  base::Callback<bool(const GURL&)> filter = builder.BuildGeneralFilter();

  TestCase test_cases[] = {
      // Whitelist matches any URL on the specified origins.
      { "https://www.google.com", true },
      { "https://www.google.com/?q=test", true },
      { "http://www.example.com", true },
      { "http://www.example.com/index.html", true },
      { "http://www.example.com/foo/bar", true },

      // Subdomains are different origins.
      { "https://test.www.google.com", false },

      // Different scheme or port is a different origin.
      { "https://www.google.com:8000", false },
      { "https://www.example.com/index.html", false },

      // Different host is a different origin.
      { "https://www.youtube.com", false },
      { "https://www.chromium.org", false },
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(OriginFilterBuilderTest, Blacklist) {
  OriginFilterBuilder builder(OriginFilterBuilder::BLACKLIST);
  builder.AddOrigin(Origin(GURL("https://www.google.com")));
  builder.AddOrigin(Origin(GURL("http://www.example.com")));
  base::Callback<bool(const GURL&)> filter = builder.BuildGeneralFilter();

  TestCase test_cases[] = {
      // URLS on explicitly specified origins are not matched.
      { "https://www.google.com", false },
      { "https://www.google.com/?q=test", false },
      { "http://www.example.com", false },
      { "http://www.example.com/index.html", false },
      { "http://www.example.com/foo/bar", false },

      // Subdomains are different origins.
      { "https://test.www.google.com", true },

      // The same hosts but with different schemes and ports
      // are not blacklisted.
      { "https://www.google.com:8000", true },
      { "https://www.example.com/index.html", true },

      // Different hosts are not blacklisted.
      { "https://www.chrome.com", true },
      { "https://www.youtube.com", true },
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

}  // namespace url
