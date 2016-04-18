// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/origin_filter_builder.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/callback.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
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

void RunTestCase(
    TestCase test_case,
    const base::Callback<bool(const ContentSettingsPattern&)>& filter) {
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString(test_case.url);
  EXPECT_TRUE(pattern.IsValid()) << test_case.url << " is not valid.";
  EXPECT_EQ(test_case.should_match, filter.Run(pattern)) << pattern.ToString();
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

TEST(OriginFilterBuilderTest, WhitelistWebsiteSettings) {
  OriginFilterBuilder builder(OriginFilterBuilder::WHITELIST);
  builder.AddOrigin(Origin(GURL("https://www.google.com")));
  builder.AddOrigin(Origin(GURL("http://www.example.com")));
  base::Callback<bool(const ContentSettingsPattern&)> filter =
      builder.BuildWebsiteSettingsPatternMatchesFilter();

  TestCase test_cases[] = {
      // Patterns of the same origins are matched.
      {"https://www.google.com:443", true},
      {"http://www.example.com:80", true},

      // Paths are always ignored during the website setting pattern creation.
      {"https://www.google.com:443/index.html?q=abc", true},
      {"http://www.example.com:80/foo/bar", true},

      // Subdomains are different origins.
      { "https://test.www.google.com", false },

      // Different scheme or port is a different origin.
      { "https://www.google.com:8000", false },
      { "https://www.example.com/index.html", false },

      // No port means port wildcard.
      {"https://www.google.com", false},
      {"http://www.example.com", false},

      // Different host is a different origin.
      { "https://www.youtube.com", false },
      { "https://www.chromium.org", false },

      // Nonstandard patterns are not matched. Note that this only documents the
      // current behavior of OriginFilterBuilder. Website settings deleted
      // by BrowsingDataRemover never use patterns scoped broader than origin.
      {"https://[*.]google.com", false},
      {"*://google.com", false},
      {"https://google.com:*", false},
      {"https://*", false},
      {"http://*", false},
      {"*", false},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(OriginFilterBuilderTest, BlacklistWebsiteSettings) {
  OriginFilterBuilder builder(OriginFilterBuilder::BLACKLIST);
  builder.AddOrigin(Origin(GURL("https://www.google.com")));
  builder.AddOrigin(Origin(GURL("http://www.example.com")));
  base::Callback<bool(const ContentSettingsPattern&)> filter =
      builder.BuildWebsiteSettingsPatternMatchesFilter();

  TestCase test_cases[] = {
      // Patterns of the same origins are matched.
      {"https://www.google.com:443", false},
      {"http://www.example.com:80", false},

      // Paths are always ignored during the website setting pattern creation.
      {"https://www.google.com:443/index.html?q=abc", false},
      {"http://www.example.com:80/foo/bar", false},

      // Subdomains are different origins.
      { "https://test.www.google.com", true },

      // Different scheme or port is a different origin.
      { "https://www.google.com:8000", true },
      { "https://www.example.com/index.html", true },

      // No port means port wildcard.
      {"https://www.google.com", true},
      {"http://www.example.com", true},

      // Different host is a different origin.
      { "https://www.youtube.com", true },
      { "https://www.chromium.org", true },

      // Nonstandard patterns are matched. Note that this only documents the
      // current behavior of OriginFilterBuilder. Website settings deleted
      // by BrowsingDataRemover never use patterns scoped broader than origin.
      {"https://[*.]google.com", true},
      {"*://google.com", true},
      {"https://google.com:*", true},
      {"https://*", true},
      {"http://*", true},
      {"*", true},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

}  // namespace url
