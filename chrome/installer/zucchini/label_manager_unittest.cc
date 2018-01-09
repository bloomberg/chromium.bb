// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/label_manager.h"

#include <utility>
#include <vector>

#include "chrome/installer/zucchini/test_reference_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

constexpr auto BAD = kUnusedIndex;
using OffsetVector = std::vector<offset_t>;

}  // namespace

TEST(LabelManagerTest, Ordered) {
  OrderedLabelManager label_manager;
  EXPECT_EQ(OffsetVector(), label_manager.Labels());
  EXPECT_EQ(BAD, label_manager.OffsetOfIndex(0));
  EXPECT_EQ(BAD, label_manager.IndexOfOffset(0));

  // Initialize with some data, test direct lookups.
  label_manager.InsertOffsets({0x33, 0x11, 0x44, 0x11});
  EXPECT_EQ(OffsetVector({0x11, 0x33, 0x44}), label_manager.Labels());

  EXPECT_EQ(0x11U, label_manager.OffsetOfIndex(0));
  EXPECT_EQ(0x33U, label_manager.OffsetOfIndex(1));
  EXPECT_EQ(0x44U, label_manager.OffsetOfIndex(2));
  EXPECT_EQ(BAD, label_manager.OffsetOfIndex(3));
  EXPECT_EQ(BAD, label_manager.OffsetOfIndex(4));

  EXPECT_EQ(0U, label_manager.IndexOfOffset(0x11));
  EXPECT_EQ(1U, label_manager.IndexOfOffset(0x33));
  EXPECT_EQ(2U, label_manager.IndexOfOffset(0x44));
  EXPECT_EQ(BAD, label_manager.IndexOfOffset(0x00));
  EXPECT_EQ(BAD, label_manager.IndexOfOffset(0x77));

  // Insert more data, note that lookup results changed.
  label_manager.InsertOffsets({0x66, 0x11, 0x11, 0x44, 0x00});
  EXPECT_EQ(OffsetVector({0x00, 0x11, 0x33, 0x44, 0x66}),
            label_manager.Labels());

  EXPECT_EQ(0x00U, label_manager.OffsetOfIndex(0));
  EXPECT_EQ(0x11U, label_manager.OffsetOfIndex(1));
  EXPECT_EQ(0x33U, label_manager.OffsetOfIndex(2));
  EXPECT_EQ(0x44U, label_manager.OffsetOfIndex(3));
  EXPECT_EQ(0x66U, label_manager.OffsetOfIndex(4));

  EXPECT_EQ(1U, label_manager.IndexOfOffset(0x11));
  EXPECT_EQ(2U, label_manager.IndexOfOffset(0x33));
  EXPECT_EQ(3U, label_manager.IndexOfOffset(0x44));
  EXPECT_EQ(0U, label_manager.IndexOfOffset(0x00));
  EXPECT_EQ(BAD, label_manager.IndexOfOffset(0x77));
}

TEST(LabelManagerTest, OrderedInsertTargets) {
  OrderedLabelManager label_manager;

  // Initialize with some data. |location| does not matter.
  TestReferenceReader reader1({{0, 0x33}, {1, 0x11}, {2, 0x44}, {3, 0x11}});
  label_manager.InsertTargets(std::move(reader1));
  EXPECT_EQ(OffsetVector({0x11, 0x33, 0x44}), label_manager.Labels());

  // Insert more data.
  TestReferenceReader reader2(
      {{0, 0x66}, {1, 0x11}, {2, 0x11}, {3, 0x44}, {4, 0x00}});
  label_manager.InsertTargets(std::move(reader2));
  EXPECT_EQ(OffsetVector({0x00, 0x11, 0x33, 0x44, 0x66}),
            label_manager.Labels());
}

TEST(LabelManagerTest, Unordered) {
  UnorderedLabelManager label_manager;
  EXPECT_EQ(BAD, label_manager.OffsetOfIndex(0));
  EXPECT_EQ(BAD, label_manager.IndexOfOffset(0));

  // Initialize with some data, test direct lookups.
  label_manager.Init(OffsetVector({0x33, BAD, BAD, 0x11, 0x44, BAD}));

  EXPECT_EQ(0x33U, label_manager.OffsetOfIndex(0));
  EXPECT_EQ(BAD, label_manager.OffsetOfIndex(1));
  EXPECT_EQ(BAD, label_manager.OffsetOfIndex(2));
  EXPECT_EQ(0x11U, label_manager.OffsetOfIndex(3));
  EXPECT_EQ(0x44U, label_manager.OffsetOfIndex(4));
  EXPECT_EQ(BAD, label_manager.OffsetOfIndex(5));
  EXPECT_EQ(BAD, label_manager.OffsetOfIndex(6));

  EXPECT_EQ(3U, label_manager.IndexOfOffset(0x11));
  EXPECT_EQ(0U, label_manager.IndexOfOffset(0x33));
  EXPECT_EQ(4U, label_manager.IndexOfOffset(0x44));
  EXPECT_EQ(BAD, label_manager.IndexOfOffset(0x00));
  EXPECT_EQ(BAD, label_manager.IndexOfOffset(0x66));

  // Insert one offset, assumed to be new.
  label_manager.InsertNewOffset(0x00);
  EXPECT_EQ(0x33U, label_manager.OffsetOfIndex(0));
  EXPECT_EQ(0x00U, label_manager.OffsetOfIndex(1));
  EXPECT_EQ(BAD, label_manager.OffsetOfIndex(2));
  EXPECT_EQ(0x11U, label_manager.OffsetOfIndex(3));
  EXPECT_EQ(0x44U, label_manager.OffsetOfIndex(4));

  EXPECT_EQ(1U, label_manager.IndexOfOffset(0x00));
  EXPECT_EQ(3U, label_manager.IndexOfOffset(0x11));
  EXPECT_EQ(0U, label_manager.IndexOfOffset(0x33));
  EXPECT_EQ(4U, label_manager.IndexOfOffset(0x44));
  EXPECT_EQ(BAD, label_manager.IndexOfOffset(0x66));

  // Insert few more offset, assumed to be new.
  label_manager.InsertNewOffset(0x22);
  label_manager.InsertNewOffset(0x77);
  label_manager.InsertNewOffset(0x55);

  EXPECT_EQ(0x33U, label_manager.OffsetOfIndex(0));
  EXPECT_EQ(0x00U, label_manager.OffsetOfIndex(1));
  EXPECT_EQ(0x22U, label_manager.OffsetOfIndex(2));
  EXPECT_EQ(0x11U, label_manager.OffsetOfIndex(3));
  EXPECT_EQ(0x44U, label_manager.OffsetOfIndex(4));
  EXPECT_EQ(0x77U, label_manager.OffsetOfIndex(5));
  EXPECT_EQ(0x55U, label_manager.OffsetOfIndex(6));

  EXPECT_EQ(1U, label_manager.IndexOfOffset(0x00));
  EXPECT_EQ(3U, label_manager.IndexOfOffset(0x11));
  EXPECT_EQ(2U, label_manager.IndexOfOffset(0x22));
  EXPECT_EQ(0U, label_manager.IndexOfOffset(0x33));
  EXPECT_EQ(4U, label_manager.IndexOfOffset(0x44));
  EXPECT_EQ(6U, label_manager.IndexOfOffset(0x55));
  EXPECT_EQ(BAD, label_manager.IndexOfOffset(0x66));
  EXPECT_EQ(5U, label_manager.IndexOfOffset(0x77));
}

}  // namespace zucchini
