// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/zucchini_apply.h"

#include <vector>

#include "chrome/installer/zucchini/image_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

constexpr auto BAD = kUnusedIndex;
using OffsetVector = std::vector<offset_t>;

}  // namespace

// Helper function wrapping MakeNewTargetsFromPatch(). |old_targets| must be
// sorted in ascending order and |equivalence| must be sorted in ascending order
// of |Equivalence::dst_offset|.
std::vector<offset_t> MakeNewTargetsFromPatchTest(
    const OffsetVector& old_targets,
    const std::vector<Equivalence>& equivalences) {
  // Serialize |equivalences| to patch format, and read it back as
  // EquivalenceSource.
  EquivalenceSink equivalence_sink;
  for (const Equivalence& equivalence : equivalences)
    equivalence_sink.PutNext(equivalence);

  std::vector<uint8_t> buffer(equivalence_sink.SerializedSize());
  BufferSink sink(buffer.data(), buffer.size());
  equivalence_sink.SerializeInto(&sink);

  BufferSource source(buffer.data(), buffer.size());
  EquivalenceSource equivalence_source;
  equivalence_source.Initialize(&source);
  return MakeNewTargetsFromPatch(old_targets, equivalence_source);
}

TEST(ZucchiniApplyTest, MakeNewTargetsFromPatch) {
  // Note that |old_offsets| provided are sorted, and |equivalences| provided
  // are sorted by |dst_offset|.

  EXPECT_EQ(OffsetVector(), MakeNewTargetsFromPatchTest({}, {}));
  EXPECT_EQ(OffsetVector({BAD, BAD}), MakeNewTargetsFromPatchTest({0, 1}, {}));

  EXPECT_EQ(OffsetVector({0, 1, BAD}),
            MakeNewTargetsFromPatchTest({0, 1, 2}, {{0, 0, 2}}));
  EXPECT_EQ(OffsetVector({1, 2, BAD}),
            MakeNewTargetsFromPatchTest({0, 1, 2}, {{0, 1, 2}}));
  EXPECT_EQ(
      OffsetVector({1, BAD, 4, 5, 6, BAD}),
      MakeNewTargetsFromPatchTest({0, 1, 2, 3, 4, 5}, {{0, 1, 1}, {2, 4, 3}}));
  EXPECT_EQ(
      OffsetVector({3, BAD, 0, 1, 2, BAD}),
      MakeNewTargetsFromPatchTest({0, 1, 2, 3, 4, 5}, {{2, 0, 3}, {0, 3, 1}}));

  // Overlap.
  EXPECT_EQ(
      OffsetVector({1, 2, 3, BAD, BAD}),
      MakeNewTargetsFromPatchTest({0, 1, 2, 3, 4}, {{0, 1, 3}, {1, 4, 2}}));
  EXPECT_EQ(
      OffsetVector({1, 4, 5, 6, BAD}),
      MakeNewTargetsFromPatchTest({0, 1, 2, 3, 4}, {{0, 1, 2}, {1, 4, 3}}));
  EXPECT_EQ(
      OffsetVector({1, 2, 5, BAD, BAD}),
      MakeNewTargetsFromPatchTest({0, 1, 2, 3, 4}, {{0, 1, 2}, {1, 4, 2}}));

  // Jump.
  EXPECT_EQ(OffsetVector({5, BAD, 6}),
            MakeNewTargetsFromPatchTest({10, 13, 15},
                                        {{0, 1, 2}, {9, 4, 2}, {15, 6, 2}}));
}

// TODO(huangs): Add more tests.

}  // namespace zucchini
