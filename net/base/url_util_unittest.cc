// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/url_util.h"

#include "base/format_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using base::WideToUTF16;

namespace net {
namespace {

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

TEST(UrlUtilTest, GetValueForKeyInQuery) {
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

TEST(UrlUtilTest, GetValueForKeyInQueryInvalidURL) {
  GURL url("http://%01/?test");
  std::string value;

  // Always false when parsing an invalid URL.
  EXPECT_FALSE(GetValueForKeyInQuery(url, "test", &value));
}

TEST(UrlUtilTest, ParseQuery) {
  const GURL url("http://example.com/path?name=value&boolParam&"
                 "url=http://test.com/q?n1%3Dv1%26n2&"
                 "multikey=value1&multikey=value2&multikey");
  QueryIterator it(url);

  ASSERT_FALSE(it.IsAtEnd());
  EXPECT_EQ("name", it.GetKey());
  EXPECT_EQ("value", it.GetValue());
  EXPECT_EQ("value", it.GetUnescapedValue());
  it.Advance();

  ASSERT_FALSE(it.IsAtEnd());
  EXPECT_EQ("boolParam", it.GetKey());
  EXPECT_EQ("", it.GetValue());
  EXPECT_EQ("", it.GetUnescapedValue());
  it.Advance();

  ASSERT_FALSE(it.IsAtEnd());
  EXPECT_EQ("url", it.GetKey());
  EXPECT_EQ("http://test.com/q?n1%3Dv1%26n2", it.GetValue());
  EXPECT_EQ("http://test.com/q?n1=v1&n2", it.GetUnescapedValue());
  it.Advance();

  ASSERT_FALSE(it.IsAtEnd());
  EXPECT_EQ("multikey", it.GetKey());
  EXPECT_EQ("value1", it.GetValue());
  EXPECT_EQ("value1", it.GetUnescapedValue());
  it.Advance();

  ASSERT_FALSE(it.IsAtEnd());
  EXPECT_EQ("multikey", it.GetKey());
  EXPECT_EQ("value2", it.GetValue());
  EXPECT_EQ("value2", it.GetUnescapedValue());
  it.Advance();

  ASSERT_FALSE(it.IsAtEnd());
  EXPECT_EQ("multikey", it.GetKey());
  EXPECT_EQ("", it.GetValue());
  EXPECT_EQ("", it.GetUnescapedValue());
  it.Advance();

  EXPECT_TRUE(it.IsAtEnd());
}

TEST(UrlUtilTest, ParseQueryInvalidURL) {
  const GURL url("http://%01/?test");
  QueryIterator it(url);
  EXPECT_TRUE(it.IsAtEnd());
}

TEST(NetUtilTest, GetIdentityFromURL) {
  struct {
    const char* const input_url;
    const char* const expected_username;
    const char* const expected_password;
  } tests[] = {
    {
      "http://username:password@google.com",
      "username",
      "password",
    },
    { // Test for http://crbug.com/19200
      "http://username:p@ssword@google.com",
      "username",
      "p@ssword",
    },
    { // Special URL characters should be unescaped.
      "http://username:p%3fa%26s%2fs%23@google.com",
      "username",
      "p?a&s/s#",
    },
    { // Username contains %20.
      "http://use rname:password@google.com",
      "use rname",
      "password",
    },
    { // Keep %00 as is.
      "http://use%00rname:password@google.com",
      "use%00rname",
      "password",
    },
    { // Use a '+' in the username.
      "http://use+rname:password@google.com",
      "use+rname",
      "password",
    },
    { // Use a '&' in the password.
      "http://username:p&ssword@google.com",
      "username",
      "p&ssword",
    },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    tests[i].input_url));
    GURL url(tests[i].input_url);

    base::string16 username, password;
    GetIdentityFromURL(url, &username, &password);

    EXPECT_EQ(ASCIIToUTF16(tests[i].expected_username), username);
    EXPECT_EQ(ASCIIToUTF16(tests[i].expected_password), password);
  }
}

// Try extracting a username which was encoded with UTF8.
TEST(UrlUtilTest, GetIdentityFromURL_UTF8) {
  GURL url(WideToUTF16(L"http://foo:\x4f60\x597d@blah.com"));

  EXPECT_EQ("foo", url.username());
  EXPECT_EQ("%E4%BD%A0%E5%A5%BD", url.password());

  // Extract the unescaped identity.
  base::string16 username, password;
  GetIdentityFromURL(url, &username, &password);

  // Verify that it was decoded as UTF8.
  EXPECT_EQ(ASCIIToUTF16("foo"), username);
  EXPECT_EQ(WideToUTF16(L"\x4f60\x597d"), password);
}

}  // namespace
}  // namespace net
