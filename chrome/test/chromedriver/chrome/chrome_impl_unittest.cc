// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/chrome_impl.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef std::list<internal::WebViewInfo> WebViewInfoList;

void ExpectEqual(const internal::WebViewInfo& info1,
                 const internal::WebViewInfo& info2) {
  EXPECT_EQ(info1.id, info2.id);
  EXPECT_EQ(info1.type, info2.type);
  EXPECT_EQ(info1.url, info2.url);
  EXPECT_EQ(info1.debugger_url, info2.debugger_url);
}

}  // namespace

TEST(ParsePagesInfo, Normal) {
  WebViewInfoList list;
  Status status = internal::ParsePagesInfo(
      "[{\"type\": \"page\", \"id\": \"1\", \"url\": \"http://page1\","
      "  \"webSocketDebuggerUrl\": \"ws://debugurl1\"}]",
      &list);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, list.size());
  ExpectEqual(
      internal::WebViewInfo(
          "1", "ws://debugurl1", "http://page1", internal::WebViewInfo::kPage),
      list.front());
}

TEST(ParsePagesInfo, Multiple) {
  WebViewInfoList list;
  Status status = internal::ParsePagesInfo(
      "[{\"type\": \"page\", \"id\": \"1\", \"url\": \"http://page1\","
      "  \"webSocketDebuggerUrl\": \"ws://debugurl1\"},"
      " {\"type\": \"other\", \"id\": \"2\", \"url\": \"http://page2\","
      "  \"webSocketDebuggerUrl\": \"ws://debugurl2\"}]",
      &list);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(2u, list.size());
  ExpectEqual(
      internal::WebViewInfo(
          "1", "ws://debugurl1", "http://page1", internal::WebViewInfo::kPage),
      list.front());
  ExpectEqual(
      internal::WebViewInfo(
          "2", "ws://debugurl2", "http://page2", internal::WebViewInfo::kOther),
      list.back());
}

TEST(ParsePagesInfo, WithoutDebuggerUrl) {
  WebViewInfoList list;
  Status status = internal::ParsePagesInfo(
      "[{\"type\": \"page\", \"id\": \"1\", \"url\": \"http://page1\"}]",
      &list);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, list.size());
  ExpectEqual(
      internal::WebViewInfo(
          "1", "", "http://page1", internal::WebViewInfo::kPage),
      list.front());
}

namespace {

void AssertFails(const std::string& data) {
  WebViewInfoList list;
  Status status = internal::ParsePagesInfo(data, &list);
  ASSERT_FALSE(status.IsOk());
  ASSERT_EQ(0u, list.size());
}

}  // namespace

TEST(ParsePagesInfo, NonList) {
  AssertFails("{\"id\": \"1\"}");
}

TEST(ParsePagesInfo, NonDictionary) {
  AssertFails("[1]");
}

TEST(ParsePagesInfo, NoId) {
  AssertFails(
      "[{\"type\": \"page\", \"url\": \"http://page1\","
      "  \"webSocketDebuggerUrl\": \"ws://debugurl1\"}]");
}

TEST(ParsePagesInfo, InvalidId) {
  AssertFails(
      "[{\"type\": \"page\", \"id\": 1, \"url\": \"http://page1\","
      "  \"webSocketDebuggerUrl\": \"ws://debugurl1\"}]");
}

TEST(ParsePagesInfo, NoType) {
  AssertFails(
      "[{\"id\": \"1\", \"url\": \"http://page1\","
      "  \"webSocketDebuggerUrl\": \"ws://debugurl1\"}]");
}

TEST(ParsePagesInfo, InvalidType) {
  AssertFails(
      "[{\"type\": \"123\", \"id\": \"1\", \"url\": \"http://page1\","
      "  \"webSocketDebuggerUrl\": \"ws://debugurl1\"}]");
}

TEST(ParsePagesInfo, NoUrl) {
  AssertFails(
      "[{\"type\": \"page\", \"id\": \"1\","
      "  \"webSocketDebuggerUrl\": \"ws://debugurl1\"}]");
}

TEST(ParsePagesInfo, InvalidUrl) {
  AssertFails(
      "[{\"type\": \"page\", \"id\": \"1\", \"url\": 1,"
      "  \"webSocketDebuggerUrl\": \"ws://debugurl1\"}]");
}

namespace {

void AssertVersionFails(const std::string& data) {
  std::string version;
  Status status = internal::ParseVersionInfo(data, &version);
  ASSERT_TRUE(status.IsError());
  ASSERT_TRUE(version.empty());
}

}  // namespace

TEST(ParseVersionInfo, InvalidJSON) {
  AssertVersionFails("[");
}

TEST(ParseVersionInfo, NonDict) {
  AssertVersionFails("[]");
}

TEST(ParseVersionInfo, NoBrowserKey) {
  AssertVersionFails("{}");
}

TEST(ParseVersionInfo, Valid) {
  std::string version;
  Status status = internal::ParseVersionInfo("{\"Browser\": \"1\"}", &version);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ("1", version);
}
