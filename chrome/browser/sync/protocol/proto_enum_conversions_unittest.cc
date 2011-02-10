// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep this file in sync with the .proto files in this directory.

#include "chrome/browser/sync/protocol/proto_enum_conversions.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace {

class ProtoEnumConversionsTest : public testing::Test {
};

template <class T>
void TestEnumStringFunction(const char* (*enum_string_fn)(T),
                            int enum_min, int enum_max) {
  for (int i = enum_min; i <= enum_max; ++i) {
    const std::string& str = enum_string_fn(static_cast<T>(i));
    EXPECT_FALSE(str.empty());
  }
}

TEST_F(ProtoEnumConversionsTest, GetBrowserTypeString) {
  TestEnumStringFunction(
      GetBrowserTypeString,
      sync_pb::SessionWindow::BrowserType_MIN,
      sync_pb::SessionWindow::BrowserType_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetPageTransitionString) {
  // We have a gap, so we need to do two ranges.
  TestEnumStringFunction(
      GetPageTransitionString,
      sync_pb::TabNavigation::PageTransition_MIN,
      sync_pb::TabNavigation::KEYWORD_GENERATED);
  TestEnumStringFunction(
      GetPageTransitionString,
      sync_pb::TabNavigation::CHAIN_START,
      sync_pb::TabNavigation::PageTransition_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetPageTransitionQualifierString) {
  TestEnumStringFunction(
      GetPageTransitionQualifierString,
      sync_pb::TabNavigation::PageTransitionQualifier_MIN,
      sync_pb::TabNavigation::PageTransitionQualifier_MAX);
}

}  // namespace
}  // namespace browser_sync
