// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_request/web_request_permissions.h"

#include "base/message_loop.h"
#include "chrome/test/base/testing_profile.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ExtensionWebRequestHelpersTest, TestHideRequestForURL) {
  MessageLoopForIO message_loop;
  TestURLRequestContext context;
  const char* sensitive_urls[] = {
      "http://www.google.com/chrome",
      "https://www.google.com/chrome",
      "http://www.google.com/chrome/foobar",
      "https://www.google.com/chrome/foobar",
      "http://chrome.google.com",
      "https://chrome.google.com",
      "http://client2.google.com",
      "https://client2.google.com",
      // No http version of webstore.
      "https://chrome.google.com/webstore",
      "http://clients2.google.com/service/update2/crx",
      "https://clients2.google.com/service/update2/crx",
      "http://www.gstatic.com/chrome/extensions/blacklist",
      "https://www.gstatic.com/chrome/extensions/blacklist",
      "notregisteredscheme://www.foobar.com"
  };
  const char* non_sensitive_urls[] = {
      "http://www.google.com/"
  };
  // Check that requests are rejected based on the destination
  for (size_t i = 0; i < arraysize(sensitive_urls); ++i) {
    GURL sensitive_url(sensitive_urls[i]);
    TestURLRequest request(sensitive_url, NULL, &context);
    EXPECT_TRUE(WebRequestPermissions::HideRequest(&request))
        << sensitive_urls[i];
  }
  // Check that requests are accepted if they don't touch sensitive urls.
  for (size_t i = 0; i < arraysize(non_sensitive_urls); ++i) {
    GURL non_sensitive_url(non_sensitive_urls[i]);
    TestURLRequest request(non_sensitive_url, NULL, &context);
    EXPECT_FALSE(WebRequestPermissions::HideRequest(&request))
        << non_sensitive_urls[i];
  }
  // Check that requests are rejected if their first party url is sensitive.
  ASSERT_GE(arraysize(non_sensitive_urls), 1u);
  GURL non_sensitive_url(non_sensitive_urls[0]);
  for (size_t i = 0; i < arraysize(sensitive_urls); ++i) {
    TestURLRequest request(non_sensitive_url, NULL, &context);
    GURL sensitive_url(sensitive_urls[i]);
    request.set_first_party_for_cookies(sensitive_url);
    EXPECT_TRUE(WebRequestPermissions::HideRequest(&request))
        << sensitive_urls[i];
  }
}
