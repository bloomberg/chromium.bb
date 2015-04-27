// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/query_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace {

TEST(QueryUtil, NoQuery) {
  GURL url("http://www.example.com/");
  std::string query;
  GURL base_url = GetBaseURLAndQuery(url, &query);
  EXPECT_EQ(base_url, url);
  EXPECT_EQ(query, "");
}

TEST(QueryUtil, WithEmptyQuery) {
  GURL url("http://www.example.com/?");
  std::string query;
  GURL base_url = GetBaseURLAndQuery(url, &query);
  EXPECT_EQ(base_url, GURL("http://www.example.com/"));
  EXPECT_EQ(query, "?");
}

TEST(QueryUtil, WithFullQuery) {
  GURL url("http://www.example.com/?a=b&c=d");
  std::string query;
  GURL base_url = GetBaseURLAndQuery(url, &query);
  EXPECT_EQ(base_url, GURL("http://www.example.com/"));
  EXPECT_EQ(query, "?a=b&c=d");
}

TEST(QueryUtil, ForFileURL) {
  GURL url("file:///tmp/file?a=b&c=d");
  std::string query;
  GURL base_url = GetBaseURLAndQuery(url, &query);
  EXPECT_EQ(base_url, GURL("file:///tmp/file"));
  EXPECT_EQ(query, "?a=b&c=d");
}

}  // namespace
}  // namespace shell
}  // namespace mojo
