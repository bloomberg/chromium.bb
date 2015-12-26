// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/leak_analyzer.h"

#include <stdint.h>

#include <algorithm>

#include "base/macros.h"
#include "components/metrics/leak_detector/custom_allocator.h"
#include "components/metrics/leak_detector/ranked_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace leak_detector {

namespace {

// Default ranking size and threshold used for leak analysis.
const int kDefaultRankedListSize = 10;
const int kDefaultLeakThreshold = 5;

// Makes it easier to instantiate LeakDetectorValueTypes. Instantiates with an
// integer value that indicates an allocation size. Storing the size allows us
// to track the storage of the LeakDetectorValueType object within LeakAnalyzer.
//
// There is no need to test this with call stacks in addition to sizes because
// call stacks will be contained in a LeakDetectorValueType object as well.
LeakDetectorValueType Size(uint32_t value) {
  return LeakDetectorValueType(value);
}

}  // namespace

class LeakAnalyzerTest : public ::testing::Test {
 public:
  LeakAnalyzerTest() {}

  void SetUp() override { CustomAllocator::Initialize(); }
  void TearDown() override { EXPECT_TRUE(CustomAllocator::Shutdown()); }

 private:
  DISALLOW_COPY_AND_ASSIGN(LeakAnalyzerTest);
};

TEST_F(LeakAnalyzerTest, Empty) {
  LeakAnalyzer analyzer(kDefaultRankedListSize, kDefaultLeakThreshold);
  EXPECT_TRUE(analyzer.suspected_leaks().empty());
}

TEST_F(LeakAnalyzerTest, SingleSize) {
  LeakAnalyzer analyzer(kDefaultRankedListSize, kDefaultLeakThreshold);

  for (int i = 0; i < kDefaultLeakThreshold + 20; ++i) {
    RankedList list(kDefaultRankedListSize);
    list.Add(Size(24), 10);
    analyzer.AddSample(std::move(list));

    // No leaks should have been detected.
    EXPECT_TRUE(analyzer.suspected_leaks().empty());
  }
}

TEST_F(LeakAnalyzerTest, VariousSizesWithoutIncrease) {
  LeakAnalyzer analyzer(kDefaultRankedListSize, kDefaultLeakThreshold);

  for (int i = 0; i < kDefaultLeakThreshold + 20; ++i) {
    RankedList list(kDefaultRankedListSize);
    list.Add(Size(24), 30);
    list.Add(Size(32), 10);
    list.Add(Size(56), 90);
    list.Add(Size(64), 40);
    analyzer.AddSample(std::move(list));

    // No leaks should have been detected.
    EXPECT_TRUE(analyzer.suspected_leaks().empty());
  }
}

TEST_F(LeakAnalyzerTest, VariousSizesWithEqualIncrease) {
  LeakAnalyzer analyzer(kDefaultRankedListSize, kDefaultLeakThreshold);

  for (int i = 0; i < kDefaultLeakThreshold + 20; ++i) {
    RankedList list(kDefaultRankedListSize);
    list.Add(Size(24), 30 + i * 10);
    list.Add(Size(32), 10 + i * 10);
    list.Add(Size(56), 90 + i * 10);
    list.Add(Size(64), 40 + i * 10);
    analyzer.AddSample(std::move(list));

    // No leaks should have been detected.
    EXPECT_TRUE(analyzer.suspected_leaks().empty());
  }
}

TEST_F(LeakAnalyzerTest, NotEnoughRunsToTriggerLeakReport) {
  LeakAnalyzer analyzer(kDefaultRankedListSize, kDefaultLeakThreshold);

  // Run this one iteration short of the number of cycles needed to trigger a
  // leak report. Because LeakAnalyzer requires |kDefaultLeakThreshold|
  // suspicions based on deltas between AddSample() calls, the below loop needs
  // to run |kDefaultLeakThreshold + 1| times to trigger a leak report.
  for (int i = 0; i <= kDefaultLeakThreshold - 1; ++i) {
    RankedList list(kDefaultRankedListSize);
    list.Add(Size(24), 30 + i * 10);  // This one has a potential leak.
    list.Add(Size(32), 10 + i * 2);
    list.Add(Size(56), 90 + i);
    list.Add(Size(64), 40 + i / 2);
    analyzer.AddSample(std::move(list));

    // No leaks should have been detected.
    EXPECT_TRUE(analyzer.suspected_leaks().empty());
  }
}

TEST_F(LeakAnalyzerTest, LeakSingleSize) {
  LeakAnalyzer analyzer(kDefaultRankedListSize, kDefaultLeakThreshold);

  // Run this past the number of iterations required to trigger a leak report.
  for (int i = 0; i < kDefaultLeakThreshold + 10; ++i) {
    RankedList list(kDefaultRankedListSize);
    list.Add(Size(32), 10);
    list.Add(Size(56), 90);
    list.Add(Size(24), 30 + i * 10);  // This one has a potential leak.
    list.Add(Size(64), 40);
    analyzer.AddSample(std::move(list));

    // No leaks should have been detected initially...
    if (i < kDefaultLeakThreshold) {
      EXPECT_TRUE(analyzer.suspected_leaks().empty());
    } else {
      // ... but there should be reported leaks once the threshold is reached.
      const auto& leaks = analyzer.suspected_leaks();
      ASSERT_EQ(1U, leaks.size());
      EXPECT_EQ(24U, leaks[0].size());
    }
  }
}

TEST_F(LeakAnalyzerTest, LeakSingleSizeOthersAlsoIncreasing) {
  LeakAnalyzer analyzer(kDefaultRankedListSize, kDefaultLeakThreshold);

  for (int i = 0; i < kDefaultLeakThreshold + 10; ++i) {
    RankedList list(kDefaultRankedListSize);
    list.Add(Size(24), 30 + i * 10);  // This one has a potential leak.
    list.Add(Size(32), 10 + i * 2);
    list.Add(Size(56), 90 + i);
    list.Add(Size(64), 40 + i / 2);
    analyzer.AddSample(std::move(list));

    // No leaks should have been detected initially...
    if (i < kDefaultLeakThreshold) {
      EXPECT_TRUE(analyzer.suspected_leaks().empty());
    } else {
      // ... but there should be reported leaks once the threshold is reached.
      const auto& leaks = analyzer.suspected_leaks();
      ASSERT_EQ(1U, leaks.size());
      EXPECT_EQ(24U, leaks[0].size());
    }
  }
}

TEST_F(LeakAnalyzerTest, LeakMultipleSizes) {
  LeakAnalyzer analyzer(kDefaultRankedListSize, kDefaultLeakThreshold);

  for (int i = 0; i < kDefaultLeakThreshold + 10; ++i) {
    RankedList list(kDefaultRankedListSize);
    list.Add(Size(24), 30 + i * 5);
    list.Add(Size(32), 10 + i * 40);
    list.Add(Size(56), 90 + i * 30);
    list.Add(Size(64), 40 + i * 20);
    list.Add(Size(80), 20 + i * 3);
    analyzer.AddSample(std::move(list));

    // No leaks should have been detected initially...
    if (i < kDefaultLeakThreshold) {
      EXPECT_TRUE(analyzer.suspected_leaks().empty());
    } else {
      // ... but there should be reported leaks once the threshold is reached.
      const auto& leaks = analyzer.suspected_leaks();
      ASSERT_EQ(3U, leaks.size());
      // These should be in order of increasing allocation size.
      EXPECT_EQ(32U, leaks[0].size());
      EXPECT_EQ(56U, leaks[1].size());
      EXPECT_EQ(64U, leaks[2].size());
    }
  }
}

TEST_F(LeakAnalyzerTest, LeakMultipleSizesValueOrder) {
  LeakAnalyzer analyzer(kDefaultRankedListSize, kDefaultLeakThreshold);

  for (int i = 0; i <= kDefaultLeakThreshold; ++i) {
    RankedList list(kDefaultRankedListSize);
    // These are similar to LeakMultipleSizes, but the relative order of
    // allocation increases is different from the relative order of sizes.
    list.Add(Size(24), 30 + i * 5);
    list.Add(Size(32), 10 + i * 20);
    list.Add(Size(56), 90 + i * 40);
    list.Add(Size(64), 40 + i * 30);
    list.Add(Size(80), 20 + i * 3);
    analyzer.AddSample(std::move(list));
  }

  const auto& leaks = analyzer.suspected_leaks();
  ASSERT_EQ(3U, leaks.size());
  // These should be in order of increasing allocation size, NOT in order of
  // allocation count or deltas.
  EXPECT_EQ(32U, leaks[0].size());
  EXPECT_EQ(56U, leaks[1].size());
  EXPECT_EQ(64U, leaks[2].size());
}

TEST_F(LeakAnalyzerTest, EqualIncreasesNoLeak) {
  LeakAnalyzer analyzer(kDefaultRankedListSize, kDefaultLeakThreshold);

  for (int i = 0; i < kDefaultLeakThreshold + 20; ++i) {
    RankedList list(kDefaultRankedListSize);
    list.Add(Size(24), 30 + i * 10);
    list.Add(Size(32), 10 + i * 10);
    list.Add(Size(56), 90 + i * 10);
    list.Add(Size(64), 40 + i * 10);
    list.Add(Size(80), 20 + i * 10);
    analyzer.AddSample(std::move(list));

    EXPECT_TRUE(analyzer.suspected_leaks().empty());
  }
}

TEST_F(LeakAnalyzerTest, NotBigEnoughDeltaGap) {
  LeakAnalyzer analyzer(kDefaultRankedListSize, kDefaultLeakThreshold);

  for (int i = 0; i < kDefaultLeakThreshold + 20; ++i) {
    RankedList list(kDefaultRankedListSize);
    // These all have different increments but there is no clear group of
    // increases that are larger than the rest.
    list.Add(Size(24), 30 + i * 80);
    list.Add(Size(32), 10 + i * 45);
    list.Add(Size(56), 90 + i * 25);
    list.Add(Size(64), 40 + i * 15);
    list.Add(Size(80), 20 + i * 10);
    analyzer.AddSample(std::move(list));

    EXPECT_TRUE(analyzer.suspected_leaks().empty());
  }
}

TEST_F(LeakAnalyzerTest, RepeatedRisesUntilLeakFound) {
  LeakAnalyzer analyzer(kDefaultRankedListSize, kDefaultLeakThreshold);

  // Remember, there is an extra iteration beyond |kDefaultLeakThreshold| needed
  // to actually trigger the leak detection.
  for (int i = 0; i <= kDefaultLeakThreshold - 2; ++i) {
    RankedList list(kDefaultRankedListSize);
    list.Add(Size(24), 30 + i * 10);
    list.Add(Size(32), 10);
    list.Add(Size(56), 90);
    list.Add(Size(64), 40);
    list.Add(Size(80), 20);
    analyzer.AddSample(std::move(list));

    EXPECT_TRUE(analyzer.suspected_leaks().empty());
  }

  // Drop back down to 30.
  for (int i = 0; i <= kDefaultLeakThreshold - 1; ++i) {
    RankedList list(kDefaultRankedListSize);
    list.Add(Size(24), 30 + i * 10);
    list.Add(Size(32), 10);
    list.Add(Size(56), 90);
    list.Add(Size(64), 40);
    list.Add(Size(80), 20);
    analyzer.AddSample(std::move(list));

    EXPECT_TRUE(analyzer.suspected_leaks().empty());
  }

  // Drop back down to 30.
  for (int i = 0; i <= kDefaultLeakThreshold; ++i) {
    // Initially there should not be any leak detected.
    EXPECT_TRUE(analyzer.suspected_leaks().empty());

    RankedList list(kDefaultRankedListSize);
    list.Add(Size(24), 30 + i * 10);
    list.Add(Size(32), 10);
    list.Add(Size(56), 90);
    list.Add(Size(64), 40);
    list.Add(Size(80), 20);
    analyzer.AddSample(std::move(list));
  }
  const auto& leaks = analyzer.suspected_leaks();
  ASSERT_EQ(1U, leaks.size());
  EXPECT_EQ(24U, leaks[0].size());
}

TEST_F(LeakAnalyzerTest, LeakWithMultipleGroupsOfDeltas) {
  const int kRankedListSize = 20;
  LeakAnalyzer analyzer(kRankedListSize, kDefaultLeakThreshold);

  for (int i = 0; i <= kDefaultLeakThreshold; ++i) {
    RankedList list(kRankedListSize);
    list.Add(Size(24), 30 + i * 10);  // A group of smaller deltas.
    list.Add(Size(32), 10 + i * 3);
    list.Add(Size(80), 20 + i * 5);
    list.Add(Size(40), 30 + i * 7);
    list.Add(Size(56), 90);
    list.Add(Size(64), 40);
    list.Add(Size(128), 100);
    list.Add(Size(44), 100 + i * 10);  // A group of medium deltas.
    list.Add(Size(16), 60 + i * 50);
    list.Add(Size(4), 20 + i * 40);
    list.Add(Size(8), 100 + i * 60);
    list.Add(Size(48), 100);
    list.Add(Size(72), 60 + i * 240);  // A group of largest deltas.
    list.Add(Size(28), 100);
    list.Add(Size(100), 100 + i * 200);
    list.Add(Size(104), 60 + i * 128);
    analyzer.AddSample(std::move(list));
  }
  // Only the group of largest deltas should be caught.
  const auto& leaks = analyzer.suspected_leaks();
  ASSERT_EQ(3U, leaks.size());
  // These should be in order of increasing allocation size.
  EXPECT_EQ(72U, leaks[0].size());
  EXPECT_EQ(100U, leaks[1].size());
  EXPECT_EQ(104U, leaks[2].size());
}

TEST_F(LeakAnalyzerTest, LeakMultipleSizesWithLargeThreshold) {
  const int kLeakThreshold = 50;
  LeakAnalyzer analyzer(kDefaultRankedListSize, kLeakThreshold);

  for (int i = 0; i <= kLeakThreshold + 10; ++i) {
    RankedList list(kDefaultRankedListSize);
    // * - Cluster of larger deltas
    list.Add(Size(24), 30 + i * 5);
    list.Add(Size(32), 10 + i * 40);  // *
    list.Add(Size(56), 90 + i * 30);  // *
    list.Add(Size(40), 30 + i * 7);
    list.Add(Size(64), 40 + i * 25);  // *
    list.Add(Size(80), 20 + i * 3);
    list.Add(Size(128), 100);
    list.Add(Size(44), 100 + i * 10);
    list.Add(Size(16), 60 + i * 50);  // *
    analyzer.AddSample(std::move(list));

    // No leaks should have been detected initially...
    if (i < kLeakThreshold) {
      EXPECT_TRUE(analyzer.suspected_leaks().empty());
    } else {
      // ... but there should be reported leaks once the threshold is reached.
      const auto& leaks = analyzer.suspected_leaks();
      ASSERT_EQ(4U, leaks.size());
      // These should be in order of increasing allocation size.
      EXPECT_EQ(16U, leaks[0].size());
      EXPECT_EQ(32U, leaks[1].size());
      EXPECT_EQ(56U, leaks[2].size());
      EXPECT_EQ(64U, leaks[3].size());
    }
  }
}

}  // namespace leak_detector
}  // namespace metrics
