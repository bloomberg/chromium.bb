// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/zucchini_gen.h"

#include <utility>

#include "base/logging.h"
#include "chrome/installer/zucchini/encoded_view.h"
#include "chrome/installer/zucchini/equivalence_map.h"
#include "chrome/installer/zucchini/image_index.h"
#include "chrome/installer/zucchini/patch_writer.h"
#include "chrome/installer/zucchini/suffix_array.h"

namespace zucchini {

namespace {

// Parameters for patch generation.
constexpr int kMinEquivalenceSimilarity = 12;

}  // namespace

std::vector<offset_t> MakeNewTargetsFromEquivalenceMap(
    const std::vector<offset_t>& old_targets,
    const std::vector<Equivalence>& equivalences) {
  auto current_equivalence = equivalences.begin();
  std::vector<offset_t> new_targets;
  new_targets.reserve(old_targets.size());
  for (offset_t src : old_targets) {
    while (current_equivalence != equivalences.end() &&
           current_equivalence->src_end() <= src)
      ++current_equivalence;

    if (current_equivalence != equivalences.end() &&
        current_equivalence->src_offset <= src) {
      // Select the longest equivalence that contains |src|. In case of a tie,
      // prefer equivalence with minimal |dst_offset|.
      auto best_equivalence = current_equivalence;
      for (auto next_equivalence = current_equivalence;
           next_equivalence != equivalences.end() &&
           src >= next_equivalence->src_offset;
           ++next_equivalence) {
        if (next_equivalence->length > best_equivalence->length ||
            (next_equivalence->length == best_equivalence->length &&
             next_equivalence->dst_offset < best_equivalence->dst_offset)) {
          // If an |next_equivalence| is longer or equal to |best_equivalence|,
          // it can be show that |src < next_equivalence->src_end()| i.e., |src|
          // is inside |next_equivalence|.
          DCHECK_LT(src, next_equivalence->src_end());
          best_equivalence = next_equivalence;
        }
      }
      new_targets.push_back(src - best_equivalence->src_offset +
                            best_equivalence->dst_offset);
    } else {
      new_targets.push_back(kUnusedIndex);
    }
  }
  return new_targets;
}

std::vector<offset_t> FindExtraTargets(
    const std::vector<Reference>& new_references,
    const EquivalenceMap& equivalence_map) {
  auto equivalence = equivalence_map.begin();
  std::vector<offset_t> targets;
  for (const Reference& ref : new_references) {
    while (equivalence != equivalence_map.end() &&
           equivalence->eq.dst_end() <= ref.location)
      ++equivalence;

    if (equivalence == equivalence_map.end())
      break;
    if (ref.location >= equivalence->eq.dst_offset && !IsMarked(ref.target))
      targets.push_back(ref.target);
  }
  return targets;
}

bool GenerateEquivalencesAndExtraData(ConstBufferView new_image,
                                      const EquivalenceMap& equivalence_map,
                                      PatchElementWriter* patch_writer) {
  // Make 2 passes through |equivalence_map| to reduce write churn.
  // Pass 1: Write all equivalences.
  EquivalenceSink equivalences_sink;
  for (const EquivalenceCandidate& candidate : equivalence_map)
    equivalences_sink.PutNext(candidate.eq);
  patch_writer->SetEquivalenceSink(std::move(equivalences_sink));

  // Pass 2: Write data in gaps in |new_image| before / between  after
  // |equivalence_map| as "extra data".
  ExtraDataSink extra_data_sink;
  offset_t dst_offset = 0;
  for (const EquivalenceCandidate& candidate : equivalence_map) {
    extra_data_sink.PutNext(
        new_image[{dst_offset, candidate.eq.dst_offset - dst_offset}]);
    dst_offset = candidate.eq.dst_end();
    DCHECK_LE(dst_offset, new_image.size());
  }
  extra_data_sink.PutNext(
      new_image[{dst_offset, new_image.size() - dst_offset}]);
  patch_writer->SetExtraDataSink(std::move(extra_data_sink));
  return true;
}

bool GenerateRawDelta(ConstBufferView old_image,
                      ConstBufferView new_image,
                      const EquivalenceMap& equivalence_map,
                      const ImageIndex& new_image_index,
                      PatchElementWriter* patch_writer) {
  RawDeltaSink raw_delta_sink;

  // Visit |equivalence_map| blocks in |new_image| order. Find and emit all
  // bytewise differences.
  offset_t base_copy_offset = 0;
  for (const EquivalenceCandidate& candidate : equivalence_map) {
    Equivalence equivalence = candidate.eq;
    // For each bytewise delta from |old_image| to |new_image|, compute "copy
    // offset" and pass it along with delta to the sink.
    for (offset_t i = 0; i < equivalence.length; ++i) {
      if (new_image_index.IsReference(equivalence.dst_offset + i))
        continue;  // Skip references since they're handled elsewhere.

      int8_t diff = new_image[equivalence.dst_offset + i] -
                    old_image[equivalence.src_offset + i];
      if (diff)
        raw_delta_sink.PutNext({base_copy_offset + i, diff});
    }
    base_copy_offset += equivalence.length;
  }
  patch_writer->SetRawDeltaSink(std::move(raw_delta_sink));
  return true;
}

bool GenerateRawElement(const std::vector<offset_t>& old_sa,
                        ConstBufferView old_image,
                        ConstBufferView new_image,
                        PatchElementWriter* patch_writer) {
  ImageIndex old_image_index(old_image, {});
  ImageIndex new_image_index(new_image, {});

  EquivalenceMap equivalences;
  equivalences.Build(old_sa, old_image_index, new_image_index,
                     kMinEquivalenceSimilarity);

  patch_writer->SetReferenceDeltaSink({});
  return GenerateEquivalencesAndExtraData(new_image, equivalences,
                                          patch_writer) &&
         GenerateRawDelta(old_image, new_image, equivalences, new_image_index,
                          patch_writer);
}

/******** Exported Functions ********/

status::Code GenerateEnsemble(ConstBufferView old_image,
                              ConstBufferView new_image,
                              EnsemblePatchWriter* patch_writer) {
  patch_writer->SetPatchType(PatchType::kEnsemblePatch);

  // Dummy patch element to fill patch_writer.
  PatchElementWriter patch_element(
      {Element(old_image.region()), Element(new_image.region())});
  patch_element.SetEquivalenceSink({});
  patch_element.SetExtraDataSink({});
  patch_element.SetRawDeltaSink({});
  patch_element.SetReferenceDeltaSink({});
  patch_writer->AddElement(std::move(patch_element));

  // TODO(etiennep): Implement.
  return status::kStatusSuccess;
}

status::Code GenerateRaw(ConstBufferView old_image,
                         ConstBufferView new_image,
                         EnsemblePatchWriter* patch_writer) {
  patch_writer->SetPatchType(PatchType::kRawPatch);

  ImageIndex old_image_index(old_image, {});
  EncodedView old_view(&old_image_index);
  std::vector<offset_t> old_sa =
      MakeSuffixArray<InducedSuffixSort>(old_view, old_view.Cardinality());

  PatchElementWriter patch_element(
      {Element(old_image.region()), Element(new_image.region())});
  if (!GenerateRawElement(old_sa, old_image, new_image, &patch_element))
    return status::kStatusFatal;
  patch_writer->AddElement(std::move(patch_element));
  return status::kStatusSuccess;
}

}  // namespace zucchini
