// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/crash_key.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace crash_reporter {
namespace {

class CrashKeyStringTest : public testing::Test {
 public:
  void SetUp() override { InitializeCrashKeys(); }
};

TEST_F(CrashKeyStringTest, ScopedCrashKeyString) {
  static CrashKeyString<32> key("test-scope");

  EXPECT_FALSE(key.is_set());

  {
    ScopedCrashKeyString scoper(&key, "value");
    EXPECT_TRUE(key.is_set());
  }

  EXPECT_FALSE(key.is_set());
}

}  // namespace
}  // namespace crash_reporter
