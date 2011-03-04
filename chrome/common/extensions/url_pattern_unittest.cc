// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "chrome/common/extensions/url_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "googleurl/src/gurl.h"

// See url_pattern.h for examples of valid and invalid patterns.

static const int kAllSchemes =
    URLPattern::SCHEME_HTTP |
    URLPattern::SCHEME_HTTPS |
    URLPattern::SCHEME_FILE |
    URLPattern::SCHEME_FTP |
    URLPattern::SCHEME_CHROMEUI;

TEST(ExtensionURLPatternTest, ParseInvalid) {
  const struct {
    const char* pattern;
    URLPattern::ParseResult expected_result;
  } kInvalidPatterns[] = {
    { "http", URLPattern::PARSE_ERROR_MISSING_SCHEME_SEPARATOR },
    { "http:", URLPattern::PARSE_ERROR_WRONG_SCHEME_SEPARATOR },
    { "http:/", URLPattern::PARSE_ERROR_WRONG_SCHEME_SEPARATOR },
    { "about://", URLPattern::PARSE_ERROR_WRONG_SCHEME_SEPARATOR },
    { "http://", URLPattern::PARSE_ERROR_EMPTY_HOST },
    { "http:///", URLPattern::PARSE_ERROR_EMPTY_HOST },
    { "http://*foo/bar", URLPattern::PARSE_ERROR_INVALID_HOST_WILDCARD },
    { "http://foo.*.bar/baz", URLPattern::PARSE_ERROR_INVALID_HOST_WILDCARD },
    { "http://fo.*.ba:123/baz", URLPattern::PARSE_ERROR_INVALID_HOST_WILDCARD },
    { "http:/bar", URLPattern::PARSE_ERROR_WRONG_SCHEME_SEPARATOR },
    { "http://bar", URLPattern::PARSE_ERROR_EMPTY_PATH },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kInvalidPatterns); ++i) {
    URLPattern pattern(URLPattern::SCHEME_ALL);
    EXPECT_EQ(kInvalidPatterns[i].expected_result,
              pattern.Parse(kInvalidPatterns[i].pattern,
                            URLPattern::PARSE_LENIENT))
        << kInvalidPatterns[i].pattern;
  }
};

TEST(ExtensionURLPatternTest, Colons) {
  const struct {
    const char* pattern;
    URLPattern::ParseResult expected_result;
  } kTestPatterns[] = {
    { "http://foo:1234/", URLPattern::PARSE_ERROR_HAS_COLON },
    { "http://foo:1234/bar", URLPattern::PARSE_ERROR_HAS_COLON },
    { "http://*.foo:1234/", URLPattern::PARSE_ERROR_HAS_COLON },
    { "http://*.foo:1234/bar", URLPattern::PARSE_ERROR_HAS_COLON },
    { "http://:1234/", URLPattern::PARSE_ERROR_HAS_COLON },
    { "http://foo:/", URLPattern::PARSE_ERROR_HAS_COLON },
    { "http://*.foo:/", URLPattern::PARSE_ERROR_HAS_COLON },
    { "http://foo:com/", URLPattern::PARSE_ERROR_HAS_COLON },

    // Port-like strings in the path should not trigger a warning.
    { "http://*/:1234", URLPattern::PARSE_SUCCESS },
    { "http://*.foo/bar:1234", URLPattern::PARSE_SUCCESS },
    { "http://foo/bar:1234/path", URLPattern::PARSE_SUCCESS },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestPatterns); ++i) {
    URLPattern pattern(URLPattern::SCHEME_ALL);

    // Without |strict_error_checks|, expect success.
    EXPECT_EQ(URLPattern::PARSE_SUCCESS,
              pattern.Parse(kTestPatterns[i].pattern,
                            URLPattern::PARSE_LENIENT))
        << "Got unexpected error for URL pattern: "
        << kTestPatterns[i].pattern;

    EXPECT_EQ(kTestPatterns[i].expected_result,
              pattern.Parse(kTestPatterns[i].pattern,
                            URLPattern::PARSE_STRICT))
        << "Got unexpected result for URL pattern: "
        << kTestPatterns[i].pattern;
  }
};

// all pages for a given scheme
TEST(ExtensionURLPatternTest, Match1) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::PARSE_SUCCESS,
            pattern.Parse("http://*/*", URLPattern::PARSE_STRICT));
  EXPECT_EQ("http", pattern.scheme());
  EXPECT_EQ("", pattern.host());
  EXPECT_TRUE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/*", pattern.path());
  EXPECT_TRUE(pattern.MatchesUrl(GURL("http://google.com")));
  EXPECT_TRUE(pattern.MatchesUrl(GURL("http://yahoo.com")));
  EXPECT_TRUE(pattern.MatchesUrl(GURL("http://google.com/foo")));
  EXPECT_FALSE(pattern.MatchesUrl(GURL("https://google.com")));
  EXPECT_TRUE(pattern.MatchesUrl(GURL("http://74.125.127.100/search")));
}

// all domains
TEST(ExtensionURLPatternTest, Match2) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::PARSE_SUCCESS,
            pattern.Parse("https://*/foo*", URLPattern::PARSE_STRICT));
  EXPECT_EQ("https", pattern.scheme());
  EXPECT_EQ("", pattern.host());
  EXPECT_TRUE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/foo*", pattern.path());
  EXPECT_TRUE(pattern.MatchesUrl(GURL("https://www.google.com/foo")));
  EXPECT_TRUE(pattern.MatchesUrl(GURL("https://www.google.com/foobar")));
  EXPECT_FALSE(pattern.MatchesUrl(GURL("http://www.google.com/foo")));
  EXPECT_FALSE(pattern.MatchesUrl(GURL("https://www.google.com/")));
}

// subdomains
TEST(URLPatternTest, Match3) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::PARSE_SUCCESS,
            pattern.Parse("http://*.google.com/foo*bar",
                          URLPattern::PARSE_STRICT));
  EXPECT_EQ("http", pattern.scheme());
  EXPECT_EQ("google.com", pattern.host());
  EXPECT_TRUE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/foo*bar", pattern.path());
  EXPECT_TRUE(pattern.MatchesUrl(GURL("http://google.com/foobar")));
  EXPECT_TRUE(pattern.MatchesUrl(GURL("http://www.google.com/foo?bar")));
  EXPECT_TRUE(pattern.MatchesUrl(
      GURL("http://monkey.images.google.com/foooobar")));
  EXPECT_FALSE(pattern.MatchesUrl(GURL("http://yahoo.com/foobar")));
}

// glob escaping
TEST(ExtensionURLPatternTest, Match5) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::PARSE_SUCCESS,
            pattern.Parse("file:///foo?bar\\*baz", URLPattern::PARSE_STRICT));
  EXPECT_EQ("file", pattern.scheme());
  EXPECT_EQ("", pattern.host());
  EXPECT_FALSE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/foo?bar\\*baz", pattern.path());
  EXPECT_TRUE(pattern.MatchesUrl(GURL("file:///foo?bar\\hellobaz")));
  EXPECT_FALSE(pattern.MatchesUrl(GURL("file:///fooXbar\\hellobaz")));
}

// ip addresses
TEST(ExtensionURLPatternTest, Match6) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::PARSE_SUCCESS,
            pattern.Parse("http://127.0.0.1/*", URLPattern::PARSE_STRICT));
  EXPECT_EQ("http", pattern.scheme());
  EXPECT_EQ("127.0.0.1", pattern.host());
  EXPECT_FALSE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/*", pattern.path());
  EXPECT_TRUE(pattern.MatchesUrl(GURL("http://127.0.0.1")));
}

// subdomain matching with ip addresses
TEST(ExtensionURLPatternTest, Match7) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::PARSE_SUCCESS,
            pattern.Parse("http://*.0.0.1/*",
                          URLPattern::PARSE_STRICT)); // allowed, but useless
  EXPECT_EQ("http", pattern.scheme());
  EXPECT_EQ("0.0.1", pattern.host());
  EXPECT_TRUE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/*", pattern.path());
  // Subdomain matching is never done if the argument has an IP address host.
  EXPECT_FALSE(pattern.MatchesUrl(GURL("http://127.0.0.1")));
};

// unicode
TEST(ExtensionURLPatternTest, Match8) {
  URLPattern pattern(kAllSchemes);
  // The below is the ASCII encoding of the following URL:
  // http://*.\xe1\x80\xbf/a\xc2\x81\xe1*
  EXPECT_EQ(URLPattern::PARSE_SUCCESS,
            pattern.Parse("http://*.xn--gkd/a%C2%81%E1*",
                          URLPattern::PARSE_STRICT));
  EXPECT_EQ("http", pattern.scheme());
  EXPECT_EQ("xn--gkd", pattern.host());
  EXPECT_TRUE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/a%C2%81%E1*", pattern.path());
  EXPECT_TRUE(pattern.MatchesUrl(
      GURL("http://abc.\xe1\x80\xbf/a\xc2\x81\xe1xyz")));
  EXPECT_TRUE(pattern.MatchesUrl(
      GURL("http://\xe1\x80\xbf/a\xc2\x81\xe1\xe1")));
};

// chrome://
TEST(ExtensionURLPatternTest, Match9) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::PARSE_SUCCESS,
            pattern.Parse("chrome://favicon/*", URLPattern::PARSE_STRICT));
  EXPECT_EQ("chrome", pattern.scheme());
  EXPECT_EQ("favicon", pattern.host());
  EXPECT_FALSE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/*", pattern.path());
  EXPECT_TRUE(pattern.MatchesUrl(GURL("chrome://favicon/http://google.com")));
  EXPECT_TRUE(pattern.MatchesUrl(GURL("chrome://favicon/https://google.com")));
  EXPECT_FALSE(pattern.MatchesUrl(GURL("chrome://history")));
};

// *://
TEST(ExtensionURLPatternTest, Match10) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::PARSE_SUCCESS,
            pattern.Parse("*://*/*", URLPattern::PARSE_STRICT));
  EXPECT_TRUE(pattern.MatchesScheme("http"));
  EXPECT_TRUE(pattern.MatchesScheme("https"));
  EXPECT_FALSE(pattern.MatchesScheme("chrome"));
  EXPECT_FALSE(pattern.MatchesScheme("file"));
  EXPECT_FALSE(pattern.MatchesScheme("ftp"));
  EXPECT_TRUE(pattern.match_subdomains());
  EXPECT_FALSE(pattern.match_all_urls());
  EXPECT_EQ("/*", pattern.path());
  EXPECT_TRUE(pattern.MatchesUrl(GURL("http://127.0.0.1")));
  EXPECT_FALSE(pattern.MatchesUrl(GURL("chrome://favicon/http://google.com")));
  EXPECT_FALSE(pattern.MatchesUrl(GURL("file:///foo/bar")));
};

// <all_urls>
TEST(ExtensionURLPatternTest, Match11) {
  URLPattern pattern(kAllSchemes);
  EXPECT_EQ(URLPattern::PARSE_SUCCESS,
            pattern.Parse("<all_urls>", URLPattern::PARSE_STRICT));
  EXPECT_TRUE(pattern.MatchesScheme("chrome"));
  EXPECT_TRUE(pattern.MatchesScheme("http"));
  EXPECT_TRUE(pattern.MatchesScheme("https"));
  EXPECT_TRUE(pattern.MatchesScheme("file"));
  EXPECT_TRUE(pattern.match_subdomains());
  EXPECT_TRUE(pattern.match_all_urls());
  EXPECT_EQ("/*", pattern.path());
  EXPECT_TRUE(pattern.MatchesUrl(GURL("chrome://favicon/http://google.com")));
  EXPECT_TRUE(pattern.MatchesUrl(GURL("http://127.0.0.1")));
  EXPECT_TRUE(pattern.MatchesUrl(GURL("file:///foo/bar")));
};

// SCHEME_ALL matches all schemes.
TEST(ExtensionURLPatternTest, Match12) {
  URLPattern pattern(URLPattern::SCHEME_ALL);
  EXPECT_EQ(URLPattern::PARSE_SUCCESS,
            pattern.Parse("<all_urls>", URLPattern::PARSE_STRICT));
  EXPECT_TRUE(pattern.MatchesScheme("chrome"));
  EXPECT_TRUE(pattern.MatchesScheme("http"));
  EXPECT_TRUE(pattern.MatchesScheme("https"));
  EXPECT_TRUE(pattern.MatchesScheme("file"));
  EXPECT_TRUE(pattern.MatchesScheme("javascript"));
  EXPECT_TRUE(pattern.MatchesScheme("data"));
  EXPECT_TRUE(pattern.MatchesScheme("about"));
  EXPECT_TRUE(pattern.MatchesScheme("chrome-extension"));
  EXPECT_TRUE(pattern.match_subdomains());
  EXPECT_TRUE(pattern.match_all_urls());
  EXPECT_EQ("/*", pattern.path());
  EXPECT_TRUE(pattern.MatchesUrl(GURL("chrome://favicon/http://google.com")));
  EXPECT_TRUE(pattern.MatchesUrl(GURL("http://127.0.0.1")));
  EXPECT_TRUE(pattern.MatchesUrl(GURL("file:///foo/bar")));
  EXPECT_TRUE(pattern.MatchesUrl(GURL("chrome://newtab")));
  EXPECT_TRUE(pattern.MatchesUrl(GURL("about:blank")));
  EXPECT_TRUE(pattern.MatchesUrl(GURL("about:version")));
  EXPECT_TRUE(pattern.MatchesUrl(
      GURL("data:text/html;charset=utf-8,<html>asdf</html>")));
};

static const struct MatchPatterns {
  const char* pattern;
  const char* matches;
} kMatch13UrlPatternTestCases[] = {
  {"about:*", "about:blank"},
  {"about:blank", "about:blank"},
  {"about:*", "about:version"},
  {"chrome-extension://*/*", "chrome-extension://FTW"},
  {"data:*", "data:monkey"},
  {"javascript:*", "javascript:atemyhomework"},
};

// SCHEME_ALL and specific schemes.
TEST(ExtensionURLPatternTest, Match13) {
  for (size_t i = 0; i < arraysize(kMatch13UrlPatternTestCases); ++i) {
    URLPattern pattern(URLPattern::SCHEME_ALL);
    EXPECT_EQ(URLPattern::PARSE_SUCCESS,
              pattern.Parse(kMatch13UrlPatternTestCases[i].pattern,
                            URLPattern::PARSE_STRICT))
        << " while parsing " << kMatch13UrlPatternTestCases[i].pattern;
    EXPECT_TRUE(pattern.MatchesUrl(
        GURL(kMatch13UrlPatternTestCases[i].matches)))
        << " while matching " << kMatch13UrlPatternTestCases[i].matches;
  }

  // Negative test.
  URLPattern pattern(URLPattern::SCHEME_ALL);
  EXPECT_EQ(URLPattern::PARSE_SUCCESS,
            pattern.Parse("data:*", URLPattern::PARSE_STRICT));
  EXPECT_FALSE(pattern.MatchesUrl(GURL("about:blank")));
};

static const struct GetAsStringPatterns {
  const char* pattern;
} kGetAsStringTestCases[] = {
  { "http://www/" },
  { "http://*/*" },
  { "chrome://*/*" },
  { "chrome://newtab/" },
  { "about:*" },
  { "about:blank" },
  { "chrome-extension://*/*" },
  { "chrome-extension://FTW/" },
  { "data:*" },
  { "data:monkey" },
  { "javascript:*" },
  { "javascript:atemyhomework" },
};

TEST(ExtensionURLPatternTest, GetAsString) {
  for (size_t i = 0; i < arraysize(kGetAsStringTestCases); ++i) {
    URLPattern pattern(URLPattern::SCHEME_ALL);
    EXPECT_EQ(URLPattern::PARSE_SUCCESS,
              pattern.Parse(kGetAsStringTestCases[i].pattern,
                            URLPattern::PARSE_STRICT));
    EXPECT_STREQ(kGetAsStringTestCases[i].pattern,
                 pattern.GetAsString().c_str());
  }
}

void TestPatternOverlap(const URLPattern& pattern1, const URLPattern& pattern2,
                        bool expect_overlap) {
  EXPECT_EQ(expect_overlap, pattern1.OverlapsWith(pattern2))
      << pattern1.GetAsString() << ", " << pattern2.GetAsString();
  EXPECT_EQ(expect_overlap, pattern2.OverlapsWith(pattern1))
      << pattern2.GetAsString() << ", " << pattern1.GetAsString();
}

TEST(ExtensionURLPatternTest, OverlapsWith) {
  URLPattern pattern1(kAllSchemes, "http://www.google.com/foo/*");
  URLPattern pattern2(kAllSchemes, "https://www.google.com/foo/*");
  URLPattern pattern3(kAllSchemes, "http://*.google.com/foo/*");
  URLPattern pattern4(kAllSchemes, "http://*.yahooo.com/foo/*");
  URLPattern pattern5(kAllSchemes, "http://www.yahooo.com/bar/*");
  URLPattern pattern6(kAllSchemes,
                      "http://www.yahooo.com/bar/baz/*");
  URLPattern pattern7(kAllSchemes, "file:///*");
  URLPattern pattern8(kAllSchemes, "*://*/*");
  URLPattern pattern9(URLPattern::SCHEME_HTTPS, "*://*/*");
  URLPattern pattern10(kAllSchemes, "<all_urls>");

  TestPatternOverlap(pattern1, pattern1, true);
  TestPatternOverlap(pattern1, pattern2, false);
  TestPatternOverlap(pattern1, pattern3, true);
  TestPatternOverlap(pattern1, pattern4, false);
  TestPatternOverlap(pattern3, pattern4, false);
  TestPatternOverlap(pattern4, pattern5, false);
  TestPatternOverlap(pattern5, pattern6, true);

  // Test that scheme restrictions work.
  TestPatternOverlap(pattern1, pattern8, true);
  TestPatternOverlap(pattern1, pattern9, false);
  TestPatternOverlap(pattern1, pattern10, true);

  // Test that '<all_urls>' includes file URLs, while scheme '*' does not.
  TestPatternOverlap(pattern7, pattern8, false);
  TestPatternOverlap(pattern7, pattern10, true);
}

TEST(ExtensionURLPatternTest, ConvertToExplicitSchemes) {
  std::vector<URLPattern> all_urls(URLPattern(
      kAllSchemes,
      "<all_urls>").ConvertToExplicitSchemes());

  std::vector<URLPattern> all_schemes(URLPattern(
      kAllSchemes,
      "*://google.com/foo").ConvertToExplicitSchemes());

  std::vector<URLPattern> monkey(URLPattern(
      URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS |
      URLPattern::SCHEME_FTP,
      "http://google.com/monkey").ConvertToExplicitSchemes());

  ASSERT_EQ(5u, all_urls.size());
  ASSERT_EQ(2u, all_schemes.size());
  ASSERT_EQ(1u, monkey.size());

  EXPECT_EQ("http://*/*", all_urls[0].GetAsString());
  EXPECT_EQ("https://*/*", all_urls[1].GetAsString());
  EXPECT_EQ("file:///*", all_urls[2].GetAsString());
  EXPECT_EQ("ftp://*/*", all_urls[3].GetAsString());
  EXPECT_EQ("chrome://*/*", all_urls[4].GetAsString());

  EXPECT_EQ("http://google.com/foo", all_schemes[0].GetAsString());
  EXPECT_EQ("https://google.com/foo", all_schemes[1].GetAsString());

  EXPECT_EQ("http://google.com/monkey", monkey[0].GetAsString());
}
