// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/user_activity_logger_delegate_ukm.h"

#include <memory>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace power {
namespace ml {

TEST(UserActivityLoggerDelegateUkmTest, CheckValues) {
  const std::vector<int> original_values = {0, 14, 15, 100};
  const std::vector<int> buckets = {0, 10, 15, 100};
  for (size_t i = 0; i < original_values.size(); ++i) {
    EXPECT_EQ(buckets[i],
              UserActivityLoggerDelegateUkm::BucketEveryFivePercents(
                  original_values[i]));
  }
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
