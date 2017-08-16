// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/binary_data_histogram.h"

#include <stddef.h>

#include <vector>

#include "chrome/installer/zucchini/buffer_view.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

TEST(BinaryDataHistogramTest, Basic) {
  constexpr double kUninitScore = -1;

  constexpr uint8_t kTestData[] = {2, 137, 42, 0, 0, 0, 7, 11, 1, 11, 255};
  const size_t n = sizeof(kTestData);
  ConstBufferView region(kTestData, n);

  std::vector<BinaryDataHistogram> prefix_histograms(n + 1);  // Short to long.
  std::vector<BinaryDataHistogram> suffix_histograms(n + 1);  // Long to short.

  for (size_t i = 0; i <= n; ++i) {
    ConstBufferView prefix(region.begin(), i);
    ConstBufferView suffix(region.begin() + i, n - i);
    // If regions are smaller than 2 bytes then it is invalid. Else valid.
    EXPECT_EQ(prefix.size() >= 2, prefix_histograms[i].Compute(prefix));
    EXPECT_EQ(suffix.size() >= 2, suffix_histograms[i].Compute(suffix));
    // IsValid() returns the same results.
    EXPECT_EQ(prefix.size() >= 2, prefix_histograms[i].IsValid());
    EXPECT_EQ(suffix.size() >= 2, suffix_histograms[i].IsValid());
  }

  // Full-prefix = full-suffix = full data.
  EXPECT_EQ(0.0, prefix_histograms[n].Distance(suffix_histograms[0]));
  EXPECT_EQ(0.0, suffix_histograms[0].Distance(prefix_histograms[n]));

  // Testing heuristics without overreliance on implementation details.

  // Strict prefixes, in increasing size. Compare against full data.
  double prev_prefix_score = kUninitScore;
  for (size_t i = 2; i < n; ++i) {
    double score = prefix_histograms[i].Distance(prefix_histograms[n]);
    // Positivity.
    EXPECT_GT(score, 0.0);
    // Symmetry.
    EXPECT_EQ(score, prefix_histograms[n].Distance(prefix_histograms[i]));
    // Distance should decrease as prefix gets nearer to full data.
    if (prev_prefix_score != kUninitScore)
      EXPECT_LT(score, prev_prefix_score);
    prev_prefix_score = score;
  }

  // Strict suffixes, in decreasing size. Compare against full data.
  double prev_suffix_score = -1;
  for (size_t i = 1; i <= n - 2; ++i) {
    double score = suffix_histograms[i].Distance(suffix_histograms[0]);
    // Positivity.
    EXPECT_GT(score, 0.0);
    // Symmetry.
    EXPECT_EQ(score, suffix_histograms[0].Distance(suffix_histograms[i]));
    // Distance should increase as suffix gets farther from full data.
    if (prev_suffix_score != kUninitScore)
      EXPECT_GT(score, prev_suffix_score);
    prev_suffix_score = score;
  }
}

}  // namespace zucchini
