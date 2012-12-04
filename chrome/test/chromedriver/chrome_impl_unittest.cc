// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <string>

#include "chrome/test/chromedriver/chrome_impl.h"
#include "chrome/test/chromedriver/status.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ParsePagesInfo, Normal) {
  std::list<std::string> urls;
  Status status = internal::ParsePagesInfo(
      "[{\"webSocketDebuggerUrl\": \"http://debugurl\"}]",
      &urls);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, urls.size());
  ASSERT_EQ("http://debugurl", urls.front());
}

TEST(ParsePagesInfo, Multiple) {
  std::list<std::string> urls;
  Status status = internal::ParsePagesInfo(
      "[{\"webSocketDebuggerUrl\": \"http://debugurl\"},"
      " {\"webSocketDebuggerUrl\": \"http://debugurl2\"}]",
      &urls);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ("http://debugurl", urls.front());
  ASSERT_EQ("http://debugurl2", urls.back());
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
