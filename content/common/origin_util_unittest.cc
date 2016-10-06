// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/origin_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

TEST(URLSchemesTest, IsOriginSecure) {
  EXPECT_TRUE(IsOriginSecure(GURL("file:///test/fun.html")));
  EXPECT_TRUE(IsOriginSecure(GURL("file:///test/")));

  EXPECT_TRUE(IsOriginSecure(GURL("https://example.com/fun.html")));
  EXPECT_FALSE(IsOriginSecure(GURL("http://example.com/fun.html")));

  EXPECT_TRUE(IsOriginSecure(GURL("wss://example.com/fun.html")));
  EXPECT_FALSE(IsOriginSecure(GURL("ws://example.com/fun.html")));

  EXPECT_TRUE(IsOriginSecure(GURL("http://localhost/fun.html")));
  EXPECT_TRUE(IsOriginSecure(GURL("http://pumpkin.localhost/fun.html")));
  EXPECT_TRUE(
      IsOriginSecure(GURL("http://crumpet.pumpkin.localhost/fun.html")));
  EXPECT_TRUE(IsOriginSecure(GURL("http://pumpkin.localhost:8080/fun.html")));
  EXPECT_TRUE(
      IsOriginSecure(GURL("http://crumpet.pumpkin.localhost:3000/fun.html")));
  EXPECT_FALSE(IsOriginSecure(GURL("http://localhost.com/fun.html")));
  EXPECT_TRUE(IsOriginSecure(GURL("https://localhost.com/fun.html")));

  EXPECT_TRUE(IsOriginSecure(GURL("http://127.0.0.1/fun.html")));
  EXPECT_TRUE(IsOriginSecure(GURL("ftp://127.0.0.1/fun.html")));
  EXPECT_TRUE(IsOriginSecure(GURL("http://127.3.0.1/fun.html")));
  EXPECT_FALSE(IsOriginSecure(GURL("http://127.example.com/fun.html")));
  EXPECT_TRUE(IsOriginSecure(GURL("https://127.example.com/fun.html")));

  EXPECT_TRUE(IsOriginSecure(GURL("http://[::1]/fun.html")));
  EXPECT_FALSE(IsOriginSecure(GURL("http://[::2]/fun.html")));
  EXPECT_FALSE(IsOriginSecure(GURL("http://[::1].example.com/fun.html")));

  EXPECT_FALSE(
      IsOriginSecure(GURL("filesystem:http://www.example.com/temporary/")));
  EXPECT_FALSE(
      IsOriginSecure(GURL("filesystem:ftp://www.example.com/temporary/")));
  EXPECT_TRUE(IsOriginSecure(GURL("filesystem:ftp://127.0.0.1/temporary/")));
  EXPECT_TRUE(
      IsOriginSecure(GURL("filesystem:https://www.example.com/temporary/")));
}

TEST(OriginUtilTest, Suborigins) {
  GURL no_suborigin_simple_url("https://b");
  GURL suborigin_simple_url("https-so://a.b");
  GURL no_suborigin_path_url("https://example.com/some/path");
  GURL suborigin_path_url("https-so://foobar.example.com/some/path");
  GURL no_suborigin_url_with_port("https://example.com:1234");
  GURL suborigin_url_with_port("https-so://foobar.example.com:1234");
  GURL no_suborigin_url_with_query("https://example.com/some/path?query");
  GURL suborigin_url_with_query(
      "https-so://foobar.example.com/some/path?query");
  GURL no_suborigin_url_with_fragment("https://example.com/some/path#fragment");
  GURL suborigin_url_with_fragment(
      "https-so://foobar.example.com/some/path#fragment");
  GURL no_suborigin_url_big(
      "https://example.com:1234/some/path?query#fragment");
  GURL suborigin_url_big(
      "https-so://foobar.example.com:1234/some/path?query#fragment");

  EXPECT_FALSE(HasSuborigin(no_suborigin_simple_url));
  EXPECT_FALSE(HasSuborigin(no_suborigin_path_url));
  EXPECT_TRUE(HasSuborigin(suborigin_simple_url));
  EXPECT_TRUE(HasSuborigin(suborigin_path_url));
  EXPECT_TRUE(HasSuborigin(suborigin_url_with_port));
  EXPECT_TRUE(HasSuborigin(suborigin_url_with_query));
  EXPECT_TRUE(HasSuborigin(suborigin_url_with_fragment));
  EXPECT_TRUE(HasSuborigin(suborigin_url_big));

  EXPECT_EQ("", SuboriginFromUrl(no_suborigin_simple_url));
  EXPECT_EQ("", SuboriginFromUrl(no_suborigin_path_url));
  EXPECT_EQ("a", SuboriginFromUrl(suborigin_simple_url));
  EXPECT_EQ("foobar", SuboriginFromUrl(suborigin_path_url));
  EXPECT_EQ("foobar", SuboriginFromUrl(suborigin_url_with_port));
  EXPECT_EQ("foobar", SuboriginFromUrl(suborigin_url_with_query));
  EXPECT_EQ("foobar", SuboriginFromUrl(suborigin_url_with_fragment));
  EXPECT_EQ("foobar", SuboriginFromUrl(suborigin_url_big));

  EXPECT_EQ(no_suborigin_simple_url,
            StripSuboriginFromUrl(no_suborigin_simple_url));
  EXPECT_EQ(no_suborigin_path_url,
            StripSuboriginFromUrl(no_suborigin_path_url));
  EXPECT_EQ(no_suborigin_simple_url,
            StripSuboriginFromUrl(suborigin_simple_url));
  EXPECT_EQ(no_suborigin_path_url, StripSuboriginFromUrl(suborigin_path_url));
  EXPECT_EQ(no_suborigin_url_with_port,
            StripSuboriginFromUrl(suborigin_url_with_port));
  EXPECT_EQ(no_suborigin_url_with_query,
            StripSuboriginFromUrl(suborigin_url_with_query));
  EXPECT_EQ(no_suborigin_url_with_fragment,
            StripSuboriginFromUrl(suborigin_url_with_fragment));
  EXPECT_EQ(no_suborigin_url_big, StripSuboriginFromUrl(suborigin_url_big));

  // Failure cases/invalid suborigins
  GURL just_dot_url("https-so://.");
  GURL empty_hostname_url("https-so://");
  GURL empty_suborigin_url("https-so://.foo");
  GURL no_dot_url("https-so://foo");
  GURL suborigin_but_empty_host_url("https-so://foo.");
  EXPECT_FALSE(HasSuborigin(just_dot_url));
  EXPECT_FALSE(HasSuborigin(empty_hostname_url));
  EXPECT_FALSE(HasSuborigin(empty_suborigin_url));
  EXPECT_FALSE(HasSuborigin(no_dot_url));
  EXPECT_FALSE(HasSuborigin(suborigin_but_empty_host_url));

  EXPECT_EQ("", SuboriginFromUrl(just_dot_url));
  EXPECT_EQ("", SuboriginFromUrl(empty_hostname_url));
  EXPECT_EQ("", SuboriginFromUrl(empty_suborigin_url));
  EXPECT_EQ("", SuboriginFromUrl(no_dot_url));
  EXPECT_EQ("", SuboriginFromUrl(suborigin_but_empty_host_url));

  EXPECT_EQ(just_dot_url, StripSuboriginFromUrl(just_dot_url));
  EXPECT_EQ(empty_hostname_url, StripSuboriginFromUrl(empty_hostname_url));
  EXPECT_EQ(empty_suborigin_url, StripSuboriginFromUrl(empty_suborigin_url));
  EXPECT_EQ(no_dot_url, StripSuboriginFromUrl(no_dot_url));
  EXPECT_EQ(suborigin_but_empty_host_url,
            StripSuboriginFromUrl(suborigin_but_empty_host_url));
}

}  // namespace content
