// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/feedback/blimp_feedback_data.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace client {
namespace {

TEST(BlimpFeedbackDataTest, IncludesBlimpIsSupported) {
  std::unordered_map<std::string, std::string> data = CreateBlimpFeedbackData();
  auto search = data.find(kFeedbackSupportedKey);
  ASSERT_TRUE(search != data.end());
  EXPECT_EQ("true", search->second);
}

}  // namespace
}  // namespace client
}  // namespace blimp
