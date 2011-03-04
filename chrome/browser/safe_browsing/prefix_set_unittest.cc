// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/prefix_set.h"

#include <algorithm>

#include "base/logging.h"
#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

SBPrefix GenPrefix() {
  return static_cast<SBPrefix>(base::RandUint64());
}

// Test that a small sparse random input works.
TEST(PrefixSetTest, Baseline) {
  std::vector<SBPrefix> prefixes;

  static const size_t kCount = 50000;

  for (size_t i = 0; i < kCount; ++i) {
    prefixes.push_back(GenPrefix());
  }

  std::sort(prefixes.begin(), prefixes.end());
  safe_browsing::PrefixSet prefix_set(prefixes);

  // Check that |GetPrefixes()| returns exactly the same set of
  // prefixes as was passed in.
  std::set<SBPrefix> check(prefixes.begin(), prefixes.end());
  std::vector<SBPrefix> prefixes_copy;
  prefix_set.GetPrefixes(&prefixes_copy);
  EXPECT_EQ(prefixes_copy.size(), check.size());
  EXPECT_TRUE(std::equal(check.begin(), check.end(), prefixes_copy.begin()));

  // Check that the set flags all of the inputs, and also check items
  // just above and below the inputs to make sure they aren't there.
  for (size_t i = 0; i < prefixes.size(); ++i) {
    EXPECT_TRUE(prefix_set.Exists(prefixes[i]));

    const SBPrefix left_sibling = prefixes[i] - 1;
    if (check.count(left_sibling) == 0)
      EXPECT_FALSE(prefix_set.Exists(left_sibling));

    const SBPrefix right_sibling = prefixes[i] + 1;
    if (check.count(right_sibling) == 0)
      EXPECT_FALSE(prefix_set.Exists(right_sibling));
  }
}

// Test that the empty set doesn't appear to have anything in it.
TEST(PrefixSetTest, Empty) {
  std::vector<SBPrefix> prefixes;
  safe_browsing::PrefixSet prefix_set(prefixes);
  for (size_t i = 0; i < 500; ++i) {
    EXPECT_FALSE(prefix_set.Exists(GenPrefix()));
  }
}

// Use artificial inputs to test various edge cases in Exists().
// Items before the lowest item aren't present.  Items after the
// largest item aren't present.  Create a sequence of items with
// deltas above and below 2^16, and make sure they're all present.
// Create a very long sequence with deltas below 2^16 to test crossing
// |kMaxRun|.
TEST(PrefixSetTest, EdgeCases) {
  std::vector<SBPrefix> prefixes;

  const SBPrefix kVeryPositive = 1000 * 1000 * 1000;
  const SBPrefix kVeryNegative = -kVeryPositive;

  // Put in a very negative prefix.
  SBPrefix prefix = kVeryNegative;
  prefixes.push_back(prefix);

  // Add a sequence with very large deltas.
  unsigned delta = 100 * 1000 * 1000;
  for (int i = 0; i < 10; ++i) {
    prefix += delta;
    prefixes.push_back(prefix);
  }

  // Add a sequence with deltas that start out smaller than the
  // maximum delta, and end up larger.  Also include some duplicates.
  delta = 256 * 256 - 100;
  for (int i = 0; i < 200; ++i) {
    prefix += delta;
    prefixes.push_back(prefix);
    prefixes.push_back(prefix);
    delta++;
  }

  // Add a long sequence with deltas smaller than the maximum delta,
  // so a new index item will be injected.
  delta = 256 * 256 - 1;
  prefix = kVeryPositive - delta * 1000;
  prefixes.push_back(prefix);
  for (int i = 0; i < 1000; ++i) {
    prefix += delta;
    prefixes.push_back(prefix);
    delta--;
  }

  std::sort(prefixes.begin(), prefixes.end());
  safe_browsing::PrefixSet prefix_set(prefixes);

  // Check that |GetPrefixes()| returns the same set of prefixes as
  // was passed in.
  std::vector<SBPrefix> prefixes_copy;
  prefix_set.GetPrefixes(&prefixes_copy);
  prefixes.erase(std::unique(prefixes.begin(), prefixes.end()), prefixes.end());
  EXPECT_EQ(prefixes_copy.size(), prefixes.size());
  EXPECT_TRUE(std::equal(prefixes.begin(), prefixes.end(),
                         prefixes_copy.begin()));

  // Items before and after the set are not present, and don't crash.
  EXPECT_FALSE(prefix_set.Exists(kVeryNegative - 100));
  EXPECT_FALSE(prefix_set.Exists(kVeryPositive + 100));

  // Check that the set correctly flags all of the inputs, and also
  // check items just above and below the inputs to make sure they
  // aren't present.
  for (size_t i = 0; i < prefixes.size(); ++i) {
    EXPECT_TRUE(prefix_set.Exists(prefixes[i]));

    EXPECT_FALSE(prefix_set.Exists(prefixes[i] - 1));
    EXPECT_FALSE(prefix_set.Exists(prefixes[i] + 1));
  }
}

}  // namespace
