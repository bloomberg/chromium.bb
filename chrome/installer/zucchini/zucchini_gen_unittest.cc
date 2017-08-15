// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/zucchini_gen.h"

#include <vector>

#include "chrome/installer/zucchini/equivalence_map.h"
#include "chrome/installer/zucchini/image_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

constexpr auto BAD = kUnusedIndex;
using OffsetVector = std::vector<offset_t>;

}  // namespace

TEST(ZucchiniGenTest, MakeNewTargetsFromEquivalenceMap) {
  // Note that |old_offsets| provided are sorted, and |equivalences| provided
  // are sorted by |src_offset|.

  EXPECT_EQ(OffsetVector(), MakeNewTargetsFromEquivalenceMap({}, {}));
  EXPECT_EQ(OffsetVector({BAD, BAD}),
            MakeNewTargetsFromEquivalenceMap({0, 1}, {}));

  EXPECT_EQ(OffsetVector({0, 1, BAD}),
            MakeNewTargetsFromEquivalenceMap({0, 1, 2}, {{0, 0, 2}}));
  EXPECT_EQ(OffsetVector({1, 2, BAD}),
            MakeNewTargetsFromEquivalenceMap({0, 1, 2}, {{0, 1, 2}}));
  EXPECT_EQ(OffsetVector({1, BAD, 4, 5, 6, BAD}),
            MakeNewTargetsFromEquivalenceMap({0, 1, 2, 3, 4, 5},
                                             {{0, 1, 1}, {2, 4, 3}}));
  EXPECT_EQ(OffsetVector({3, BAD, 0, 1, 2, BAD}),
            MakeNewTargetsFromEquivalenceMap({0, 1, 2, 3, 4, 5},
                                             {{0, 3, 1}, {2, 0, 3}}));

  // Overlap in src.
  EXPECT_EQ(OffsetVector({1, 2, 3, BAD, BAD}),
            MakeNewTargetsFromEquivalenceMap({0, 1, 2, 3, 4},
                                             {{0, 1, 3}, {1, 4, 2}}));
  EXPECT_EQ(OffsetVector({1, 4, 5, 6, BAD}),
            MakeNewTargetsFromEquivalenceMap({0, 1, 2, 3, 4},
                                             {{0, 1, 2}, {1, 4, 3}}));
  EXPECT_EQ(OffsetVector({1, 2, 5, BAD, BAD}),
            MakeNewTargetsFromEquivalenceMap({0, 1, 2, 3, 4},
                                             {{0, 1, 2}, {1, 4, 2}}));

  // Jump in src.
  EXPECT_EQ(OffsetVector({5, BAD, 6}),
            MakeNewTargetsFromEquivalenceMap(
                {10, 13, 15}, {{0, 1, 2}, {9, 4, 2}, {15, 6, 2}}));
}

TEST(ZucchiniGenTest, FindExtraTargets) {
  // Note that |new_offsets| provided are sorted, and |equivalences| provided
  // are sorted by |dst_offset|.

  EXPECT_EQ(OffsetVector(), FindExtraTargets({}, {}));
  EXPECT_EQ(OffsetVector(), FindExtraTargets({{0, 0}}, {}));
  EXPECT_EQ(OffsetVector(), FindExtraTargets({{0, IsMarked(0)}}, {}));

  EXPECT_EQ(OffsetVector({0}),
            FindExtraTargets({{0, 0}}, EquivalenceMap({{{0, 0, 2}, 0.0}})));
  EXPECT_EQ(OffsetVector(),
            FindExtraTargets({{0, MarkIndex(0)}},
                             EquivalenceMap({{{0, 0, 2}, 0.0}})));

  EXPECT_EQ(OffsetVector({1, 2}),
            FindExtraTargets({{0, 0}, {1, 1}, {2, 2}, {3, 3}},
                             EquivalenceMap({{{0, 1, 2}, 0.0}})));
  EXPECT_EQ(OffsetVector({2}),
            FindExtraTargets({{0, 0}, {1, MarkIndex(1)}, {2, 2}, {3, 3}},
                             EquivalenceMap({{{0, 1, 2}, 0.0}})));

  EXPECT_EQ(
      OffsetVector({1, 2, 4, 5}),
      FindExtraTargets({{0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 6}},
                       EquivalenceMap({{{0, 1, 2}, 0.0}, {{0, 4, 2}, 0.0}})));
  EXPECT_EQ(
      OffsetVector({4, 5}),
      FindExtraTargets({{3, 3}, {4, 4}, {5, 5}, {6, 6}},
                       EquivalenceMap({{{0, 1, 2}, 0.0}, {{0, 4, 2}, 0.0}})));
}

// TODO(huangs): Add more tests.

}  // namespace zucchini
