// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/label_manager.h"

#include <iterator>
#include <map>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace courgette {

namespace {

// Test version of RvaVisitor: Just wrap std::vector<RVA>.
class TestRvaVisitor : public LabelManager::RvaVisitor {
 public:
  explicit TestRvaVisitor(std::vector<RVA>::const_iterator rva_begin,
                          std::vector<RVA>::const_iterator rva_end)
      : rva_it_(rva_begin), rva_end_(rva_end) {}

  ~TestRvaVisitor() override {}

  size_t Remaining() const override { return std::distance(rva_it_, rva_end_); }

  RVA Get() const override { return *rva_it_; }

  void Next() override { ++rva_it_; }

 private:
  std::vector<RVA>::const_iterator rva_it_;
  std::vector<RVA>::const_iterator rva_end_;
};

// Test version of LabelManager: Expose data to test implementation.
class TestLabelManager : public LabelManager {
 public:
  size_t LabelCount() const { return labels_.size(); };
};

void CheckLabelManagerContent(TestLabelManager* label_manager,
                              const std::map<RVA, int32>& expected) {
  EXPECT_EQ(expected.size(), label_manager->LabelCount());
  for (const auto& rva_and_count : expected) {
    Label* label = label_manager->Find(rva_and_count.first);
    EXPECT_TRUE(label != nullptr);
    EXPECT_EQ(rva_and_count.first, label->rva_);
    EXPECT_EQ(rva_and_count.second, label->count_);
  }
}

}  // namespace

TEST(LabelManagerTest, Basic) {
  static const RVA kTestTargetsRaw[] = {
    0x04000010,
    0x04000030,
    0x04000020,
    0x04000010,  // Redundant
    0xFEEDF00D,
    0x04000030,  // Redundant
    0xFEEDF00D,  // Redundant
    0x00000110,
    0x04000010,  // Redundant
    0xABCD1234
  };
  std::vector<RVA> test_targets(std::begin(kTestTargetsRaw),
                                std::end(kTestTargetsRaw));
  TestRvaVisitor visitor(test_targets.begin(), test_targets.end());

  // Preallocate targets, then populate.
  TestLabelManager label_manager;
  label_manager.Read(&visitor);

  static const std::pair<RVA, int32> kExpected1Raw[] = {
    {0x00000110, 1},
    {0x04000010, 3},
    {0x04000020, 1},
    {0x04000030, 2},
    {0xABCD1234, 1},
    {0xFEEDF00D, 2}
  };
  std::map<RVA, int32> expected1(std::begin(kExpected1Raw),
                                 std::end(kExpected1Raw));

  CheckLabelManagerContent(&label_manager, expected1);

  // Expect to *not* find labels for various RVAs that were never added.
  EXPECT_EQ(nullptr, label_manager.Find(RVA(0x00000000)));
  EXPECT_EQ(nullptr, label_manager.Find(RVA(0x0400000F)));
  EXPECT_EQ(nullptr, label_manager.Find(RVA(0x04000011)));
  EXPECT_EQ(nullptr, label_manager.Find(RVA(0x5F3759DF)));
  EXPECT_EQ(nullptr, label_manager.Find(RVA(0xFEEDFFF0)));
  EXPECT_EQ(nullptr, label_manager.Find(RVA(0xFFFFFFFF)));

  // Remove Labels with |count_| < 2.
  label_manager.RemoveUnderusedLabels(2);
  static const std::pair<RVA, int32> kExpected2Raw[] = {
    {0x04000010, 3},
    {0x04000030, 2},
    {0xFEEDF00D, 2}
  };
  std::map<RVA, int32> expected2(std::begin(kExpected2Raw),
                                 std::end(kExpected2Raw));
  CheckLabelManagerContent(&label_manager, expected2);
}

TEST(LabelManagerTest, Single) {
  const RVA kRva = 12U;
  for (int dup = 1; dup < 8; ++dup) {
    // Test data: |dup| copies of kRva.
    std::vector<RVA> test_targets(dup, kRva);
    TestRvaVisitor visitor(test_targets.begin(), test_targets.end());
    TestLabelManager label_manager;
    label_manager.Read(&visitor);
    EXPECT_EQ(1U, label_manager.LabelCount());  // Deduped to 1 Label.

    Label* label = label_manager.Find(kRva);
    EXPECT_NE(nullptr, label);
    EXPECT_EQ(kRva, label->rva_);
    EXPECT_EQ(dup, label->count_);

    for (RVA rva = 0U; rva < 16U; ++rva) {
      if (rva != kRva)
        EXPECT_EQ(nullptr, label_manager.Find(rva));
    }
  }
}

TEST(LabelManagerTest, Empty) {
  std::vector<RVA> empty_test_targets;
  TestRvaVisitor visitor(empty_test_targets.begin(), empty_test_targets.end());
  TestLabelManager label_manager;
  label_manager.Read(&visitor);
  EXPECT_EQ(0U, label_manager.LabelCount());
  for (RVA rva = 0U; rva < 16U; ++rva)
    EXPECT_EQ(nullptr, label_manager.Find(rva));
}

}  // namespace courgette
