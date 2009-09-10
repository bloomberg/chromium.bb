// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/all_status.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

TEST(AllStatus, GetRecommendedDelay) {
  EXPECT_LE(0, AllStatus::GetRecommendedDelaySeconds(0));
  EXPECT_LE(1, AllStatus::GetRecommendedDelaySeconds(1));
  EXPECT_LE(50, AllStatus::GetRecommendedDelaySeconds(50));
  EXPECT_LE(10, AllStatus::GetRecommendedDelaySeconds(10));
  EXPECT_EQ(AllStatus::kMaxBackoffSeconds,
            AllStatus::GetRecommendedDelaySeconds(
                AllStatus::kMaxBackoffSeconds));
  EXPECT_EQ(AllStatus::kMaxBackoffSeconds,
            AllStatus::GetRecommendedDelaySeconds(
                AllStatus::kMaxBackoffSeconds+1));
}

}  // namespace browser_sync
