// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/zucchini_gen.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "chrome/installer/zucchini/equivalence_map.h"
#include "chrome/installer/zucchini/image_index.h"
#include "chrome/installer/zucchini/image_utils.h"
#include "chrome/installer/zucchini/label_manager.h"
#include "chrome/installer/zucchini/test_disassembler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

using OffsetVector = std::vector<offset_t>;

// In normal usage, 0.0 is an unrealistic similarity value for an
// EquivalenceCandiate. Since similarity doesn't affect results for various unit
// tests in this file, we use this dummy value for simplicity.
constexpr double kDummySim = 0.0;

// Helper function wrapping GenerateReferencesDelta().
std::vector<int32_t> GenerateReferencesDeltaTest(
    const std::vector<Reference>& old_references,
    const std::vector<Reference>& new_references,
    std::vector<offset_t>&& exp_old_targets,
    std::vector<offset_t>&& new_labels,
    const EquivalenceMap& equivalence_map) {
  ReferenceDeltaSink reference_delta_sink;

  TargetPool old_targets;
  old_targets.InsertTargets(old_references);
  EXPECT_EQ(exp_old_targets, old_targets.targets());
  ReferenceSet old_refs({1, TypeTag(0), PoolTag(0)}, old_targets);
  old_refs.InitReferences(old_references);

  TargetPool new_targets;
  new_targets.InsertTargets(new_references);
  ReferenceSet new_refs({1, TypeTag(0), PoolTag(0)}, new_targets);
  new_refs.InitReferences(new_references);

  UnorderedLabelManager label_manager;
  label_manager.Init(std::move(new_labels));

  GenerateReferencesDelta(old_refs, new_refs, label_manager, equivalence_map,
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

// Helper function wrapping FindExtraTargets(). |new_references| take rvalue so
// callers can be simplified.
std::vector<offset_t> FindExtraTargetsTest(
    std::vector<Reference>&& new_references,
    std::vector<offset_t>&& new_labels,
    EquivalenceMap&& equivalence_map) {
  TargetPool new_targets;
  new_targets.InsertTargets(new_references);
  ReferenceSet reference_set({1, TypeTag(0), PoolTag(0)}, new_targets);
  reference_set.InitReferences(new_references);

  UnorderedLabelManager new_label_manager;
  new_label_manager.Init(std::move(new_labels));
  return FindExtraTargets(reference_set, new_label_manager, equivalence_map);
}

}  // namespace

TEST(ZucchiniGenTest, MakeNewTargetsFromEquivalenceMap) {
  // Note that |old_offsets| provided are sorted, and |equivalences| provided
  // are sorted by |src_offset|.

  constexpr auto kBad = kUnusedIndex;

  EXPECT_EQ(OffsetVector(), MakeNewTargetsFromEquivalenceMap({}, {}));
  EXPECT_EQ(OffsetVector({kBad, kBad}),
            MakeNewTargetsFromEquivalenceMap({0, 1}, {}));

  EXPECT_EQ(OffsetVector({0, 1, kBad}),
            MakeNewTargetsFromEquivalenceMap({0, 1, 2}, {{0, 0, 2}}));
  EXPECT_EQ(OffsetVector({1, 2, kBad}),
            MakeNewTargetsFromEquivalenceMap({0, 1, 2}, {{0, 1, 2}}));
  EXPECT_EQ(OffsetVector({1, kBad, 4, 5, 6, kBad}),
            MakeNewTargetsFromEquivalenceMap({0, 1, 2, 3, 4, 5},
                                             {{0, 1, 1}, {2, 4, 3}}));
  EXPECT_EQ(OffsetVector({3, kBad, 0, 1, 2, kBad}),
            MakeNewTargetsFromEquivalenceMap({0, 1, 2, 3, 4, 5},
                                             {{0, 3, 1}, {2, 0, 3}}));

  // Overlap in src.
  EXPECT_EQ(OffsetVector({1, 2, 3, kBad, kBad}),
            MakeNewTargetsFromEquivalenceMap({0, 1, 2, 3, 4},
                                             {{0, 1, 3}, {1, 4, 2}}));
  EXPECT_EQ(OffsetVector({1, 4, 5, 6, kBad}),
            MakeNewTargetsFromEquivalenceMap({0, 1, 2, 3, 4},
                                             {{0, 1, 2}, {1, 4, 3}}));
  EXPECT_EQ(OffsetVector({1, 2, 5, kBad, kBad}),
            MakeNewTargetsFromEquivalenceMap({0, 1, 2, 3, 4},
                                             {{0, 1, 2}, {1, 4, 2}}));

  // Jump in src.
  EXPECT_EQ(OffsetVector({5, kBad, 6}),
            MakeNewTargetsFromEquivalenceMap(
                {10, 13, 15}, {{0, 1, 2}, {9, 4, 2}, {15, 6, 2}}));

  // Tie-breaking: Prefer longest Equivalence, and then Equivalence with min
  // |dst_offset|.
  EXPECT_EQ(OffsetVector({11}),
            MakeNewTargetsFromEquivalenceMap({1}, {{0, 10, 4}, {1, 21, 3}}));
  EXPECT_EQ(OffsetVector({21}),
            MakeNewTargetsFromEquivalenceMap({1}, {{0, 10, 3}, {1, 21, 4}}));
  EXPECT_EQ(OffsetVector({11}),
            MakeNewTargetsFromEquivalenceMap({1}, {{0, 10, 4}, {1, 21, 4}}));
}

TEST(ZucchiniGenTest, FindExtraTargets) {
  // Note that |new_offsets| provided are sorted, and |equivalences| provided
  // are sorted by |dst_offset|.

  // No equivalences.
  EXPECT_EQ(OffsetVector(), FindExtraTargetsTest({}, {}, {}));
  EXPECT_EQ(OffsetVector(), FindExtraTargetsTest({{20, 0}}, {}, {}));
  EXPECT_EQ(OffsetVector(),
            FindExtraTargetsTest({{20, 0}}, {kUnusedIndex}, {}));

  // Unrelated equivalence.
  EXPECT_EQ(OffsetVector(),
            FindExtraTargetsTest({{20, 0}, {23, 0}}, {},
                                 EquivalenceMap({{{12, 21, 2}, kDummySim}})));

  // Simple cases with one reference.
  EXPECT_EQ(OffsetVector({1}),
            FindExtraTargetsTest({{20, 1}}, {},
                                 EquivalenceMap({{{10, 20, 2}, kDummySim}})));
  EXPECT_EQ(OffsetVector({1}),
            FindExtraTargetsTest({{20, 1}}, {kUnusedIndex},
                                 EquivalenceMap({{{10, 20, 2}, kDummySim}})));
  // With symmetry.
  EXPECT_EQ(OffsetVector({1}),
            FindExtraTargetsTest({{10, 1}}, {kUnusedIndex},
                                 EquivalenceMap({{{10, 10, 2}, kDummySim}})));

  // Target ignored because it is not "extra" (found as label).
  EXPECT_EQ(OffsetVector({}),
            FindExtraTargetsTest({{20, 1}}, {1},
                                 EquivalenceMap({{{10, 20, 2}, kDummySim}})));
  EXPECT_EQ(OffsetVector({}),
            FindExtraTargetsTest({{20, 1}}, {2, 1},
                                 EquivalenceMap({{{10, 20, 2}, kDummySim}})));
  EXPECT_EQ(OffsetVector({}),
            FindExtraTargetsTest({{20, 1}}, {kUnusedIndex, 1},
                                 EquivalenceMap({{{10, 20, 2}, kDummySim}})));

  // Simple cases with multiple references.
  EXPECT_EQ(OffsetVector({5, 3}),
            FindExtraTargetsTest({{20, 4}, {21, 5}, {22, 3}, {23, 6}}, {},
                                 EquivalenceMap({{{10, 21, 2}, kDummySim}})));
  // With unrelated new labels.
  EXPECT_EQ(OffsetVector({5, 3}),
            FindExtraTargetsTest({{20, 4}, {21, 5}, {22, 3}, {23, 6}},
                                 {kUnusedIndex, 9},
                                 EquivalenceMap({{{10, 21, 2}, kDummySim}})));
  // With unrelated equivalences.
  EXPECT_EQ(OffsetVector({5, 3}),
            FindExtraTargetsTest({{20, 4}, {21, 5}, {22, 3}, {23, 6}}, {},
                                 EquivalenceMap({{{11, 11, 2}, kDummySim},
                                                 {{10, 21, 2}, kDummySim}})));
  EXPECT_EQ(OffsetVector({5, 3}),
            FindExtraTargetsTest({{20, 4}, {21, 5}, {22, 3}, {23, 6}}, {},
                                 EquivalenceMap({{{10, 21, 2}, kDummySim},
                                                 {{11, 31, 2}, kDummySim}})));

  // Multiple equivalences. Targets are ordered for simplicity.
  EXPECT_EQ(OffsetVector({1, 2, 4, 5}),
            FindExtraTargetsTest(
                {{20, 0}, {21, 1}, {22, 2}, {23, 3}, {24, 4}, {25, 5}, {26, 6}},
                {kUnusedIndex},
                EquivalenceMap(
                    {{{10, 21, 2}, kDummySim}, {{10, 24, 2}, kDummySim}})));
  EXPECT_EQ(OffsetVector({2, 4}),
            FindExtraTargetsTest(
                {{20, 0}, {21, 1}, {22, 2}, {23, 3}, {24, 4}, {25, 5}, {26, 6}},
                {5, kUnusedIndex, 1},
                EquivalenceMap(
                    {{{10, 21, 2}, kDummySim}, {{10, 24, 2}, kDummySim}})));
  EXPECT_EQ(OffsetVector({4, 5}),
            FindExtraTargetsTest({{23, 3}, {24, 4}, {25, 5}, {26, 6}}, {},
                                 EquivalenceMap({{{10, 21, 2}, kDummySim},
                                                 {{10, 24, 2}, kDummySim}})));
}

TEST(ZucchiniGenTest, GenerateReferencesDelta) {
  // No equivalences.
  EXPECT_EQ(std::vector<int32_t>(),
            GenerateReferencesDeltaTest({}, {}, {}, {}, EquivalenceMap()));
  EXPECT_EQ(std::vector<int32_t>(),
            GenerateReferencesDeltaTest({{10, 0}}, {{20, 0}}, {0}, {},
                                        EquivalenceMap()));

  // Simple cases with one equivalence.
  EXPECT_EQ(
      std::vector<int32_t>({0}),  // {0 - 0}.
      GenerateReferencesDeltaTest({{10, 3}}, {{20, 3}}, {3}, {3, 4},
                                  EquivalenceMap({{{10, 20, 4}, kDummySim}})));
  EXPECT_EQ(
      std::vector<int32_t>({1}),  // {1 - 0}.
      GenerateReferencesDeltaTest({{10, 3}}, {{20, 3}}, {3}, {4, 3},
                                  EquivalenceMap({{{10, 20, 4}, kDummySim}})));
  EXPECT_EQ(std::vector<int32_t>({2, -1}),  // {2 - 0, 0 - 1}.
            GenerateReferencesDeltaTest(
                {{10, 3}, {11, 4}}, {{20, 3}, {21, 4}}, {3, 4}, {4, 5, 3},
                EquivalenceMap({{{10, 20, 4}, kDummySim}})));

  EXPECT_EQ(std::vector<int32_t>({0, 0}),  // {1 - 1, 2 - 2}.
            GenerateReferencesDeltaTest(
                {{10, 3}, {11, 4}, {12, 5}, {13, 6}},
                {{20, 3}, {21, 4}, {22, 5}, {23, 6}}, {3, 4, 5, 6},
                {3, 4, 5, 6}, EquivalenceMap({{{11, 21, 2}, kDummySim}})));

  // Multiple equivalences.
  EXPECT_EQ(std::vector<int32_t>({-1, 1}),  // {0 - 1, 1 - 0}.
            GenerateReferencesDeltaTest(
                {{10, 0}, {12, 1}}, {{10, 0}, {12, 1}}, {0, 1}, {0, 1},
                EquivalenceMap(
                    {{{12, 10, 2}, kDummySim}, {{10, 12, 2}, kDummySim}})));
  EXPECT_EQ(
      std::vector<int32_t>({0, 0}),  // {0 - 0, 1 - 1}.
      GenerateReferencesDeltaTest(
          {{0, 0}, {2, 1}}, {{0, 0}, {2, 1}}, {0, 1}, {1, 0},
          EquivalenceMap({{{2, 0, 2}, kDummySim}, {{0, 2, 2}, kDummySim}})));

  EXPECT_EQ(std::vector<int32_t>({-2, 2}),  // {0 - 2, 2 - 0}.
            GenerateReferencesDeltaTest(
                {{10, 0}, {12, 1}, {14, 2}}, {{10, 0}, {12, 1}, {14, 2}},
                {0, 1, 2}, {0, 1, 2},
                EquivalenceMap(
                    {{{14, 10, 2}, kDummySim}, {{10, 14, 2}, kDummySim}})));

  EXPECT_EQ(std::vector<int32_t>({-2, 2}),  // {0 - 2, 2 - 0}.
            GenerateReferencesDeltaTest(
                {{11, 0}, {14, 1}, {17, 2}}, {{11, 0}, {14, 1}, {17, 2}},
                {0, 1, 2}, {0, 1, 2},
                EquivalenceMap(
                    {{{16, 10, 3}, kDummySim}, {{10, 16, 3}, kDummySim}})));

  EXPECT_EQ(
      std::vector<int32_t>({-2, 2}),  // {0 - 2, 2 - 0}.
      GenerateReferencesDeltaTest({{10, 0}, {14, 2}, {16, 1}},
                                  {{10, 0}, {14, 2}}, {0, 1, 2}, {0, 1, 2},
                                  EquivalenceMap({{{14, 10, 2}, kDummySim},
                                                  {{12, 12, 2}, kDummySim},
                                                  {{10, 14, 2}, kDummySim}})));
}

// TODO(huangs): Add more tests.

}  // namespace zucchini
