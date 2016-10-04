// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/helium/helium_result.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace {

class HeliumResultTest : public testing::Test {
 public:
  HeliumResultTest() {}
  ~HeliumResultTest() override {}
};

TEST_F(HeliumResultTest, HeliumResultToString) {
  // The exhaustive list of errors need not be specified here, but enough are
  // specified that we can verify that the switch/case mapping works as
  // intended.
  EXPECT_STREQ("SUCCESS", HeliumResultToString(HeliumResult::SUCCESS));
  EXPECT_STREQ("ERR_INTERNAL_ERROR",
               HeliumResultToString(HeliumResult::ERR_INTERNAL_ERROR));
  EXPECT_STREQ("ERR_DISCONNECTED",
               HeliumResultToString(HeliumResult::ERR_DISCONNECTED));
}

}  // namespace
}  // namespace blimp
