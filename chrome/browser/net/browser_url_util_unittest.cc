// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/browser_url_util.h"

#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_browser_net {

TEST(BrowserUrlUtilTest, AppendQueryParameter) {
  // Appending a name-value pair to a URL without a query component.
  EXPECT_EQ("http://example.com/path?name=value",
            AppendQueryParameter(GURL("http://example.com/path"),
                                 "name", "value").spec());

  // Appending a name-value pair to a URL with a query component.
  // The original component should be preserved, and the new pair should be
  // appended with '&'.
  EXPECT_EQ("http://example.com/path?existing=one&name=value",
            AppendQueryParameter(GURL("http://example.com/path?existing=one"),
                                 "name", "value").spec());

  // Appending a name-value pair with unsafe characters included. The
  // unsafe characters should be escaped.
  EXPECT_EQ("http://example.com/path?existing=one&na+me=v.alue%3D",
            AppendQueryParameter(GURL("http://example.com/path?existing=one"),
                                 "na me", "v.alue=").spec());
}

TEST(BrowserUrlUtilTest, GetValueForKeyInQuery) {
  GURL url("http://example.com/path?name=value&boolParam&"
           "url=http://test.com/q?n1%3Dv1%26n2");
  std::string value;

  // False when getting a non-existent query param.
  EXPECT_FALSE(GetValueForKeyInQuery(url, "non-exist", &value));

  // True when query param exist.
  EXPECT_TRUE(GetValueForKeyInQuery(url, "name", &value));
  EXPECT_EQ("value", value);

  EXPECT_TRUE(GetValueForKeyInQuery(url, "boolParam", &value));
  EXPECT_EQ("", value);

  EXPECT_TRUE(GetValueForKeyInQuery(url, "url", &value));
  EXPECT_EQ("http://test.com/q?n1=v1&n2", value);
}

}  // namespace chrome_browser_net.
