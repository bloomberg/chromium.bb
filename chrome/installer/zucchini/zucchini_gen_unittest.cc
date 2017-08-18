// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/zucchini_gen.h"

#include <vector>

#include "chrome/installer/zucchini/equivalence_map.h"
#include "chrome/installer/zucchini/image_index.h"
#include "chrome/installer/zucchini/image_utils.h"
#include "chrome/installer/zucchini/test_reference_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

constexpr auto BAD = kUnusedIndex;
using OffsetVector = std::vector<offset_t>;

// Make all references 2 bytes long.
constexpr offset_t kReferenceSize = 2;

// Creates and initialize an ImageIndex from |a| and with 2 types of references.
// The result is populated with |refs0| and |refs1|. |a| is expected to be a
// string literal valid for the lifetime of the object.
// TODO(huangs): Update this to use TestDisassembler.
ImageIndex MakeImageIndexForTesting(const char* a,
                                    const std::vector<Reference>& refs0,
                                    const std::vector<Reference>& refs1) {
  std::vector<ReferenceTypeTraits> traits(
      {ReferenceTypeTraits{kReferenceSize, TypeTag(0), PoolTag(0)},
       ReferenceTypeTraits{kReferenceSize, TypeTag(1), PoolTag(0)}});

  ImageIndex image_index(
      ConstBufferView(reinterpret_cast<const uint8_t*>(a), std::strlen(a)),
      std::move(traits));

  EXPECT_TRUE(
      image_index.InsertReferences(TypeTag(0), TestReferenceReader(refs0)));
  EXPECT_TRUE(
      image_index.InsertReferences(TypeTag(1), TestReferenceReader(refs1)));
  return image_index;
}

// Helper function wrapping GenerateReferencesDelta().
std::vector<int32_t> GenerateReferencesDeltaTest(
    const ImageIndex& old_index,
    const ImageIndex& new_index,
    const EquivalenceMap& equivalence_map) {
  ReferenceDeltaSink reference_delta_sink;
  GenerateReferencesDelta(old_index, new_index, equivalence_map,
                          &reference_delta_sink);

  // Serialize |reference_delta_sink| to patch format, and read it back as
  // std::vector<int32_t>.
  std::vector<uint8_t> buffer(reference_delta_sink.SerializedSize());
  BufferSink sink(buffer.data(), buffer.size());
  reference_delta_sink.SerializeInto(&sink);

  BufferSource source(buffer.data(), buffer.size());
  ReferenceDeltaSource reference_delta_source;
  EXPECT_TRUE(reference_delta_source.Initialize(&source));
  std::vector<int32_t> delta_vec;
  for (auto delta = reference_delta_source.GetNext(); delta.has_value();
       delta = reference_delta_source.GetNext()) {
    delta_vec.push_back(*delta);
  }
  EXPECT_TRUE(reference_delta_source.Done());
  return delta_vec;
}

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

TEST(ZucchiniGenTest, GenerateReferencesDelta) {
  EXPECT_EQ(std::vector<int32_t>(),
            GenerateReferencesDeltaTest(MakeImageIndexForTesting("", {}, {}),
                                        MakeImageIndexForTesting("", {}, {}),
                                        EquivalenceMap()));

  EXPECT_EQ(std::vector<int32_t>(),
            GenerateReferencesDeltaTest(
                MakeImageIndexForTesting("XXYY", {{0, 0}}, {{2, 1}}),
                MakeImageIndexForTesting("XXYY", {{0, 0}}, {{2, 1}}),
                EquivalenceMap()));

  EXPECT_EQ(std::vector<int32_t>({0, 0}),
            GenerateReferencesDeltaTest(
                MakeImageIndexForTesting("XXYY", {{0, MarkIndex(0)}},
                                         {{2, MarkIndex(1)}}),
                MakeImageIndexForTesting("XXYY", {{0, MarkIndex(0)}},
                                         {{2, MarkIndex(1)}}),
                EquivalenceMap({{{0, 0, 4}, 0.0}})));

  EXPECT_EQ(std::vector<int32_t>({1, -1}),
            GenerateReferencesDeltaTest(
                MakeImageIndexForTesting("XXYY", {{0, MarkIndex(0)}},
                                         {{2, MarkIndex(1)}}),
                MakeImageIndexForTesting("XXYY", {{0, MarkIndex(1)}},
                                         {{2, MarkIndex(0)}}),
                EquivalenceMap({{{0, 0, 4}, 0.0}})));

  EXPECT_EQ(std::vector<int32_t>({1, -1}),
            GenerateReferencesDeltaTest(
                MakeImageIndexForTesting(
                    "aXXbYYc", {{1, MarkIndex(0)}, {4, MarkIndex(1)}}, {}),
                MakeImageIndexForTesting(
                    "aXXbYYc", {{1, MarkIndex(1)}, {4, MarkIndex(0)}}, {}),
                EquivalenceMap({{{0, 0, 7}, 0.0}})));

  EXPECT_EQ(std::vector<int32_t>({-1, 1}),
            GenerateReferencesDeltaTest(
                MakeImageIndexForTesting(
                    "XXYY", {{0, MarkIndex(0)}, {2, MarkIndex(1)}}, {}),
                MakeImageIndexForTesting(
                    "XXYY", {{0, MarkIndex(0)}, {2, MarkIndex(1)}}, {}),
                EquivalenceMap({{{2, 0, 2}, 0.0}, {{0, 2, 2}, 0.0}})));

  EXPECT_EQ(
      std::vector<int32_t>({-2, 2}),
      GenerateReferencesDeltaTest(
          MakeImageIndexForTesting(
              "XXYYZZ",
              {{0, MarkIndex(0)}, {2, MarkIndex(1)}, {4, MarkIndex(2)}}, {}),
          MakeImageIndexForTesting(
              "XXYYZZ",
              {{0, MarkIndex(0)}, {2, MarkIndex(1)}, {4, MarkIndex(2)}}, {}),
          EquivalenceMap({{{4, 0, 2}, 0.0}, {{0, 4, 2}, 0.0}})));

  EXPECT_EQ(
      std::vector<int32_t>({-2, 2}),
      GenerateReferencesDeltaTest(
          MakeImageIndexForTesting(
              "aXXbYYcZZd",
              {{1, MarkIndex(0)}, {4, MarkIndex(1)}, {7, MarkIndex(2)}}, {}),
          MakeImageIndexForTesting(
              "aXXbYYcZZd",
              {{1, MarkIndex(0)}, {4, MarkIndex(1)}, {7, MarkIndex(2)}}, {}),
          EquivalenceMap({{{6, 0, 3}, 0.0}, {{0, 6, 3}, 0.0}})));

  EXPECT_EQ(std::vector<int32_t>({-2, 2}),
            GenerateReferencesDeltaTest(
                MakeImageIndexForTesting(
                    "XXabZZ", {{0, MarkIndex(0)}, {4, MarkIndex(2)}}, {}),
                MakeImageIndexForTesting(
                    "XXabZZ", {{0, MarkIndex(0)}, {4, MarkIndex(2)}}, {}),
                EquivalenceMap(
                    {{{4, 0, 2}, 0.0}, {{2, 2, 2}, 0.0}, {{0, 4, 2}, 0.0}})));
}

// TODO(huangs): Add more tests.

}  // namespace zucchini
