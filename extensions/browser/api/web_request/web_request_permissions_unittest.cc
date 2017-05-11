// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_permissions.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

TEST(ExtensionWebRequestPermissions, IsSensitiveURL) {
  struct TestCase {
    const char* url;
    bool is_sensitive_if_request_from_common_renderer;
    bool is_sensitive_if_request_from_browser_or_webui_renderer;
  } cases[] = {
      {"http://www.google.com", false, false},
      {"http://www.example.com", false, false},
      {"http://clients.google.com", false, true},
      {"http://clients4.google.com", false, true},
      {"http://clients9999.google.com", false, true},
      {"http://clients9999..google.com", false, false},
      {"http://clients9999.example.google.com", false, false},
      {"http://clients.google.com.", false, true},
      {"http://.clients.google.com.", false, true},
      {"http://google.example.com", false, false},
      {"http://www.example.com", false, false},
      {"http://clients.google.com", false, true},
      {"http://sb-ssl.google.com", true, true},
      {"http://sb-ssl.random.google.com", false, false},
      {"http://chrome.google.com", false, false},
      {"http://chrome.google.com/webstore", true, true},
      {"http://chrome.google.com/webstore?query", true, true},
  };
  for (const TestCase& test : cases) {
    EXPECT_EQ(
        test.is_sensitive_if_request_from_common_renderer,
        IsSensitiveURL(GURL(test.url),
                       false /* is_request_from_browser_or_webui_renderer */))
        << test.url;
    EXPECT_EQ(
        test.is_sensitive_if_request_from_browser_or_webui_renderer,
        IsSensitiveURL(GURL(test.url),
                       true /* is_request_from_browser_or_webui_renderer */))
        << test.url;
  }
}

}  // namespace extensions
