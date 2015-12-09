// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/ranked_list.h"

#include <algorithm>

#include "base/macros.h"
#include "components/metrics/leak_detector/custom_allocator.h"
#include "components/metrics/leak_detector/leak_detector_value_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace leak_detector {

namespace {

// Makes it easier to instantiate LeakDetectorValueTypes.
LeakDetectorValueType Value(uint32_t value) {
  return LeakDetectorValueType(value);
}

}  // namespace

class RankedListTest : public ::testing::Test {
 public:
  RankedListTest() {}

  void SetUp() override { CustomAllocator::Initialize(); }
  void TearDown() override { EXPECT_TRUE(CustomAllocator::Shutdown()); }

 private:
  DISALLOW_COPY_AND_ASSIGN(RankedListTest);
};

TEST_F(RankedListTest, Iterators) {
  RankedList list(10);
  EXPECT_TRUE(list.begin() == list.end());

  list.Add(Value(0x1234), 100);
  EXPECT_FALSE(list.begin() == list.end());
}

TEST_F(RankedListTest, SingleInsertion) {
  RankedList list(10);
  EXPECT_EQ(0U, list.size());

  list.Add(Value(0x1234), 100);
  EXPECT_EQ(1U, list.size());

  auto iter = list.begin();
  EXPECT_EQ(0x1234U, iter->value.size());
  EXPECT_EQ(100, iter->count);
}

TEST_F(RankedListTest, InOrderInsertion) {
  RankedList list(10);
  EXPECT_EQ(0U, list.size());

  list.Add(Value(0x1234), 100);
  EXPECT_EQ(1U, list.size());
  list.Add(Value(0x2345), 95);
  EXPECT_EQ(2U, list.size());
  list.Add(Value(0x3456), 90);
  EXPECT_EQ(3U, list.size());
  list.Add(Value(0x4567), 85);
  EXPECT_EQ(4U, list.size());
  list.Add(Value(0x5678), 80);
  EXPECT_EQ(5U, list.size());

  // Iterate through the contents to make sure they match what went in.
  const RankedList::Entry kExpectedValues[] = {
      {Value(0x1234), 100}, {Value(0x2345), 95}, {Value(0x3456), 90},
      {Value(0x4567), 85},  {Value(0x5678), 80},
  };

  size_t index = 0;
  for (const auto& entry : list) {
    EXPECT_LT(index, arraysize(kExpectedValues));
    EXPECT_EQ(kExpectedValues[index].value.size(), entry.value.size());
    EXPECT_EQ(kExpectedValues[index].count, entry.count);
    ++index;
  }
}

TEST_F(RankedListTest, ReverseOrderInsertion) {
  RankedList list(10);
  EXPECT_EQ(0U, list.size());

  list.Add(Value(0x1234), 0);
  EXPECT_EQ(1U, list.size());
  list.Add(Value(0x2345), 5);
  EXPECT_EQ(2U, list.size());
  list.Add(Value(0x3456), 10);
  EXPECT_EQ(3U, list.size());
  list.Add(Value(0x4567), 15);
  EXPECT_EQ(4U, list.size());
  list.Add(Value(0x5678), 20);
  EXPECT_EQ(5U, list.size());

  // Iterate through the contents to make sure they match what went in.
  const RankedList::Entry kExpectedValues[] = {
      {Value(0x5678), 20}, {Value(0x4567), 15}, {Value(0x3456), 10},
      {Value(0x2345), 5},  {Value(0x1234), 0},
  };

  size_t index = 0;
  for (const auto& entry : list) {
    EXPECT_LT(index, arraysize(kExpectedValues));
    EXPECT_EQ(kExpectedValues[index].value.size(), entry.value.size());
    EXPECT_EQ(kExpectedValues[index].count, entry.count);
    ++index;
  }
}

TEST_F(RankedListTest, UnorderedInsertion) {
  RankedList list(10);
  EXPECT_EQ(0U, list.size());

  list.Add(Value(0x1234), 15);
  list.Add(Value(0x2345), 20);
  list.Add(Value(0x3456), 10);
  list.Add(Value(0x4567), 30);
  list.Add(Value(0x5678), 25);
  EXPECT_EQ(5U, list.size());

  // Iterate through the contents to make sure they match what went in.
  const RankedList::Entry kExpectedValues1[] = {
      {Value(0x4567), 30}, {Value(0x5678), 25}, {Value(0x2345), 20},
      {Value(0x1234), 15}, {Value(0x3456), 10},
  };

  size_t index = 0;
  for (const auto& entry : list) {
    EXPECT_LT(index, arraysize(kExpectedValues1));
    EXPECT_EQ(kExpectedValues1[index].value.size(), entry.value.size());
    EXPECT_EQ(kExpectedValues1[index].count, entry.count);
    ++index;
  }

  // Add more items.
  list.Add(Value(0x6789), 35);
  list.Add(Value(0x789a), 40);
  list.Add(Value(0x89ab), 50);
  list.Add(Value(0x9abc), 5);
  list.Add(Value(0xabcd), 0);
  EXPECT_EQ(10U, list.size());

  // Iterate through the contents to make sure they match what went in.
  const RankedList::Entry kExpectedValues2[] = {
      {Value(0x89ab), 50}, {Value(0x789a), 40}, {Value(0x6789), 35},
      {Value(0x4567), 30}, {Value(0x5678), 25}, {Value(0x2345), 20},
      {Value(0x1234), 15}, {Value(0x3456), 10}, {Value(0x9abc), 5},
      {Value(0xabcd), 0},
  };

  index = 0;
  for (const auto& entry : list) {
    EXPECT_LT(index, arraysize(kExpectedValues2));
    EXPECT_EQ(kExpectedValues2[index].value.size(), entry.value.size());
    EXPECT_EQ(kExpectedValues2[index].count, entry.count);
    ++index;
  }
}

TEST_F(RankedListTest, InsertionWithOverflow) {
  RankedList list(5);
  EXPECT_EQ(0U, list.size());

  list.Add(Value(0x1234), 15);
  list.Add(Value(0x2345), 20);
  list.Add(Value(0x3456), 10);
  list.Add(Value(0x4567), 30);
  list.Add(Value(0x5678), 25);
  EXPECT_EQ(5U, list.size());

  // These values will not make it into the list, which is now full.
  list.Add(Value(0x6789), 0);
  EXPECT_EQ(5U, list.size());
  list.Add(Value(0x789a), 5);
  EXPECT_EQ(5U, list.size());

  // Iterate through the contents to make sure they match what went in.
  const RankedList::Entry kExpectedValues1[] = {
      {Value(0x4567), 30}, {Value(0x5678), 25}, {Value(0x2345), 20},
      {Value(0x1234), 15}, {Value(0x3456), 10},
  };

  size_t index = 0;
  for (const auto& entry : list) {
    EXPECT_LT(index, arraysize(kExpectedValues1));
    EXPECT_EQ(kExpectedValues1[index].value.size(), entry.value.size());
    EXPECT_EQ(kExpectedValues1[index].count, entry.count);
    ++index;
  }

  // Insert some more values that go in the middle of the list.
  list.Add(Value(0x89ab), 27);
  EXPECT_EQ(5U, list.size());
  list.Add(Value(0x9abc), 22);
  EXPECT_EQ(5U, list.size());

  // Iterate through the contents to make sure they match what went in.
  const RankedList::Entry kExpectedValues2[] = {
      {Value(0x4567), 30}, {Value(0x89ab), 27}, {Value(0x5678), 25},
      {Value(0x9abc), 22}, {Value(0x2345), 20},
  };

  index = 0;
  for (const auto& entry : list) {
    EXPECT_LT(index, arraysize(kExpectedValues2));
    EXPECT_EQ(kExpectedValues2[index].value.size(), entry.value.size());
    EXPECT_EQ(kExpectedValues2[index].count, entry.count);
    ++index;
  }

  // Insert some more values at the front of the list.
  list.Add(Value(0xabcd), 40);
  EXPECT_EQ(5U, list.size());
  list.Add(Value(0xbcde), 35);
  EXPECT_EQ(5U, list.size());

  // Iterate through the contents to make sure they match what went in.
  const RankedList::Entry kExpectedValues3[] = {
      {Value(0xabcd), 40}, {Value(0xbcde), 35}, {Value(0x4567), 30},
      {Value(0x89ab), 27}, {Value(0x5678), 25},
  };

  index = 0;
  for (const auto& entry : list) {
    EXPECT_LT(index, arraysize(kExpectedValues3));
    EXPECT_EQ(kExpectedValues3[index].value.size(), entry.value.size());
    EXPECT_EQ(kExpectedValues3[index].count, entry.count);
    ++index;
  }
}

TEST_F(RankedListTest, MoveOperation) {
  const RankedList::Entry kExpectedValues[] = {
      {Value(0x89ab), 50}, {Value(0x789a), 40}, {Value(0x6789), 35},
      {Value(0x4567), 30}, {Value(0x5678), 25}, {Value(0x2345), 20},
      {Value(0x1234), 15}, {Value(0x3456), 10}, {Value(0x9abc), 5},
      {Value(0xabcd), 0},
  };

  RankedList source_list(10);
  for (const RankedList::Entry& entry : kExpectedValues) {
    source_list.Add(entry.value, entry.count);
  }
  EXPECT_EQ(10U, source_list.size());

  RankedList dest_list(25);  // This should be changed by the move.
  dest_list = std::move(source_list);
  EXPECT_EQ(10U, dest_list.size());
  EXPECT_EQ(10U, dest_list.max_size());

  size_t index = 0;
  for (const auto& entry : dest_list) {
    EXPECT_LT(index, arraysize(kExpectedValues));
    EXPECT_EQ(kExpectedValues[index].value.size(), entry.value.size());
    EXPECT_EQ(kExpectedValues[index].count, entry.count);
    ++index;
  }
}

}  // namespace leak_detector
}  // namespace metrics
