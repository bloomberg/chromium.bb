// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/google_captcha_observer.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "url/gurl.h"

class GoogleCaptchaPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {};

TEST_F(GoogleCaptchaPageLoadMetricsObserverTest, IsGoogleCaptcha) {
  struct {
    std::string url;
    bool expected;
  } test_cases[] = {
      {"", false},
      {"http://www.google.com/", false},
      {"http://www.cnn.com/", false},
      {"http://ipv4.google.com/", false},
      {"https://ipv4.google.com/sorry/IndexRedirect?continue=http://a", true},
      {"https://ipv6.google.com/sorry/IndexRedirect?continue=http://a", true},
      {"https://ipv7.google.com/sorry/IndexRedirect?continue=http://a", false},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(test.expected,
              google_captcha_observer::IsGoogleCaptcha(GURL(test.url)))
        << "for URL: " << test.url;
  }
}
