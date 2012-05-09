// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/url_util.h"

#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_common_net {

TEST(UrlUtilTest, AppendQueryParameter) {
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

TEST(UrlUtilTest, AppendOrReplaceQueryParameter) {
  // Appending a name-value pair to a URL without a query component.
  EXPECT_EQ("http://example.com/path?name=value",
            AppendOrReplaceQueryParameter(GURL("http://example.com/path"),
                                 "name", "value").spec());

  // Appending a name-value pair to a URL with a query component.
  // The original component should be preserved, and the new pair should be
  // appended with '&'.
  EXPECT_EQ("http://example.com/path?existing=one&name=value",
      AppendOrReplaceQueryParameter(
          GURL("http://example.com/path?existing=one"),
          "name", "value").spec());

  // Appending a name-value pair with unsafe characters included. The
  // unsafe characters should be escaped.
  EXPECT_EQ("http://example.com/path?existing=one&na+me=v.alue%3D",
      AppendOrReplaceQueryParameter(
          GURL("http://example.com/path?existing=one"),
          "na me", "v.alue=").spec());

  // Replace value of an existing paramater.
  EXPECT_EQ("http://example.com/path?existing=one&name=new",
      AppendOrReplaceQueryParameter(
          GURL("http://example.com/path?existing=one&name=old"),
          "name", "new").spec());

  // Replace a name-value pair with unsafe characters included. The
  // unsafe characters should be escaped.
  EXPECT_EQ("http://example.com/path?na+me=n.ew%3D&existing=one",
      AppendOrReplaceQueryParameter(
          GURL("http://example.com/path?na+me=old&existing=one"),
          "na me", "n.ew=").spec());

  // Replace the value of first parameter with this name only.
  EXPECT_EQ("http://example.com/path?name=new&existing=one&name=old",
      AppendOrReplaceQueryParameter(
          GURL("http://example.com/path?name=old&existing=one&name=old"),
          "name", "new").spec());

  // Preserve the content of the original params regarless of our failure to
  // interpret them correctly.
  EXPECT_EQ("http://example.com/path?bar&name=new&left=&"
            "=right&=&&name=again",
      AppendOrReplaceQueryParameter(
          GURL("http://example.com/path?bar&name=old&left=&"
                "=right&=&&name=again"),
          "name", "new").spec());
}

}  // namespace chrome_common_net.
