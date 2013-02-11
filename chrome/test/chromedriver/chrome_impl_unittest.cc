// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome_impl.h"
#include "chrome/test/chromedriver/devtools_client.h"
#include "chrome/test/chromedriver/status.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ParsePagesInfo, Normal) {
  std::list<std::string> ids;
  Status status = internal::ParsePagesInfo(
      "[{\"type\": \"page\", \"id\":\"1\","
      "  \"webSocketDebuggerUrl\": \"http://debugurl\"}]",
      &ids);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, ids.size());
  ASSERT_EQ("1", ids.front());
}

TEST(ParsePagesInfo, Multiple) {
  std::list<std::string> ids;
  Status status = internal::ParsePagesInfo(
      "[{\"type\": \"page\", \"id\":\"1\","
      "  \"webSocketDebuggerUrl\": \"http://debugurl\"},"
      " {\"type\": \"page\", \"id\":\"2\","
      "  \"webSocketDebuggerUrl\": \"http://debugurl2\"}]",
      &ids);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(2u, ids.size());
  ASSERT_EQ("1", ids.front());
  ASSERT_EQ("2", ids.back());
}

TEST(ParsePagesInfo, WithoutDebuggerUrl) {
  std::list<std::string> ids;
  Status status = internal::ParsePagesInfo(
      "[{\"type\": \"page\", \"id\":\"1\","
      "  \"webSocketDebuggerUrl\": \"http://debugurl\"},"
      " {\"type\": \"page\", \"id\":\"2\"}]",
      &ids);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(2u, ids.size());
  ASSERT_EQ("1", ids.front());
  ASSERT_EQ("2", ids.back());
}

namespace {

void AssertFails(const std::string& data) {
  std::list<std::string> urls;
  Status status = internal::ParsePagesInfo(data, &urls);
  ASSERT_FALSE(status.IsOk());
  ASSERT_EQ(0u, urls.size());
}

}  // namespace

TEST(ParsePagesInfo, InvalidJSON) {
  AssertFails("[");
}

TEST(ParsePagesInfo, NonList) {
  AssertFails("{}");
}

TEST(ParsePagesInfo, NonDictionary) {
  AssertFails("[1]");
}

TEST(ParsePagesInfo, NoDebuggerUrl) {
  AssertFails("[{\"hi\": 1}]");
}

TEST(ParsePagesInfo, InvalidDebuggerUrl) {
  AssertFails("[{\"webSocketDebuggerUrl\": 1}]");
}
