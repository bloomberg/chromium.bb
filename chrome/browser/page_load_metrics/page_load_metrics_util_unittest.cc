// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class PageLoadMetricsUtilTest : public testing::Test {};

TEST_F(PageLoadMetricsUtilTest, IsGoogleHostname) {
  struct {
    bool expected_result;
    const char* url;
  } test_cases[] = {
      {true, "https://google.com/"},
      {true, "https://google.com/index.html"},
      {true, "https://www.google.com/"},
      {true, "https://www.google.com/search"},
      {true, "https://www.google.com/a/b/c/d"},
      {true, "https://www.google.co.uk/"},
      {true, "https://www.google.co.in/"},
      {true, "https://other.google.com/"},
      {true, "https://other.www.google.com/"},
      {true, "https://www.other.google.com/"},
      {true, "https://www.www.google.com/"},
      {false, ""},
      {false, "a"},
      {false, "*"},
      {false, "com"},
      {false, "co.uk"},
      {false, "google"},
      {false, "google.com"},
      {false, "www.google.com"},
      {false, "https:///"},
      {false, "https://a/"},
      {false, "https://*/"},
      {false, "https://com/"},
      {false, "https://co.uk/"},
      {false, "https://google/"},
      {false, "https://*.com/"},
      {false, "https://www.*.com/"},
      {false, "https://www.google.appspot.com/"},
      {false, "https://www.google.example.com/"},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(test.expected_result,
              page_load_metrics::IsGoogleHostname(GURL(test.url)))
        << "For URL: " << test.url;
  }
}

TEST_F(PageLoadMetricsUtilTest, GetGoogleHostnamePrefix) {
  struct {
    bool expected_result;
    const char* expected_prefix;
    const char* url;
  } test_cases[] = {
      {false, "", "https://example.com/"},
      {true, "", "https://google.com/"},
      {true, "www", "https://www.google.com/"},
      {true, "news", "https://news.google.com/"},
      {true, "www", "https://www.google.co.uk/"},
      {true, "other", "https://other.google.com/"},
      {true, "other.www", "https://other.www.google.com/"},
      {true, "www.other", "https://www.other.google.com/"},
      {true, "www.www", "https://www.www.google.com/"},
  };
  for (const auto& test : test_cases) {
    base::Optional<std::string> result =
        page_load_metrics::GetGoogleHostnamePrefix(GURL(test.url));
    EXPECT_EQ(test.expected_result, result.has_value())
        << "For URL: " << test.url;
    if (result) {
      EXPECT_EQ(test.expected_prefix, result.value())
          << "Prefix for URL: " << test.url;
    }
  }
}
