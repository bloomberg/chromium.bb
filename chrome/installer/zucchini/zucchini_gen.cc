// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/zucchini_gen.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/installer/zucchini/disassembler.h"
#include "chrome/installer/zucchini/element_detection.h"
#include "chrome/installer/zucchini/encoded_view.h"
#include "chrome/installer/zucchini/ensemble_matcher.h"
#include "chrome/installer/zucchini/equivalence_map.h"
#include "chrome/installer/zucchini/heuristic_ensemble_matcher.h"
#include "chrome/installer/zucchini/image_index.h"
#include "chrome/installer/zucchini/label_manager.h"
#include "chrome/installer/zucchini/patch_writer.h"
#include "chrome/installer/zucchini/suffix_array.h"
#include "chrome/installer/zucchini/targets_affinity.h"

namespace zucchini {

namespace {

// Parameters for patch generation.
constexpr double kMinEquivalenceSimilarity = 12.0;
constexpr double kMinLabelAffinity = 64.0;
constexpr size_t kNumIterations = 2;

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
    const ReferenceSet& new_references,
    const UnorderedLabelManager& new_label_manager,
    const EquivalenceMap& equivalence_map) {
  auto equivalence = equivalence_map.begin();
  std::vector<offset_t> targets;
  for (const IndirectReference& ref : new_references) {
    while (equivalence != equivalence_map.end() &&
           equivalence->eq.dst_end() <= ref.location) {
      ++equivalence;
    }
    if (equivalence == equivalence_map.end())
      break;
    if (ref.location >= equivalence->eq.dst_offset) {
      offset_t target_offset =
          new_references.target_pool().OffsetForKey(ref.target_key);
      if (!new_label_manager.ContainsOffset(target_offset))
        targets.push_back(target_offset);
    }
  }
  return targets;
}

EquivalenceMap CreateEquivalenceMap(const ImageIndex& old_image_index,
                                    const ImageIndex& new_image_index) {
  // Label matching (between "old" and "new") can guide EquivalenceMap
  // construction; but EquivalenceMap induces Label matching. This apparent
  // "chick and egg" problem is solved by multiple iterations alternating 2
  // steps:
  // - Association of targets based on previous EquivalenceMap. Note that the
  //   EquivalenceMap is empty on first iteration, so this is a no-op.
  // - Construction of refined EquivalenceMap based on new targets associations.
  size_t pool_count = old_image_index.PoolCount();
  // |target_affinities| is outside the loop to reduce allocation.
  std::vector<TargetsAffinity> target_affinities(pool_count);

  EquivalenceMap equivalence_map;
  for (size_t i = 0; i < kNumIterations; ++i) {
    EncodedView old_view(old_image_index);
    EncodedView new_view(new_image_index);

    // Associate targets from "old" to "new" image based on |equivalence_map|
    // for each reference pool.
    for (const auto& old_pool_tag_and_targets :
         old_image_index.target_pools()) {
      PoolTag pool_tag = old_pool_tag_and_targets.first;
      target_affinities[pool_tag.value()].InferFromSimilarities(
          equivalence_map, old_pool_tag_and_targets.second.targets(),
          new_image_index.pool(pool_tag).targets());

      // Creates labels for strongly associated targets.
      std::vector<uint32_t> old_labels;
      std::vector<uint32_t> new_labels;
      size_t label_bound = target_affinities[pool_tag.value()].AssignLabels(
          kMinLabelAffinity, &old_labels, &new_labels);
      old_view.SetLabels(pool_tag, std::move(old_labels), label_bound);
      new_view.SetLabels(pool_tag, std::move(new_labels), label_bound);
    }
    // Build equivalence map, where references in "old" and "new" that share
    // common semantics (i.e., their respective targets were associated earlier
    // on) are considered equivalent.
    equivalence_map.Build(
        MakeSuffixArray<InducedSuffixSort>(old_view, old_view.Cardinality()),
        old_view, new_view, target_affinities, kMinEquivalenceSimilarity);
  }

  return equivalence_map;
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

bool GenerateReferencesDelta(const ReferenceSet& src_refs,
                             const ReferenceSet& dst_refs,
                             const UnorderedLabelManager& new_label_manager,
                             const EquivalenceMap& equivalence_map,
                             ReferenceDeltaSink* reference_delta_sink) {
  offset_t ref_width = src_refs.width();
  auto dst_ref = dst_refs.begin();

  // For each equivalence, for each covered |dst_ref| and the matching
  // |src_ref|, emit the delta between the respective target labels. Note: By
  // construction, each reference location (with |ref_width|) lies either
  // completely inside an equivalence or completely outside. We perform
  // "straddle checks" throughout to verify this assertion.
  for (const auto& candidate : equivalence_map) {
    const Equivalence equiv = candidate.eq;
    // Increment |dst_ref| until it catches up to |equiv|.
    while (dst_ref != dst_refs.end() && dst_ref->location < equiv.dst_offset)
      ++dst_ref;
    if (dst_ref == dst_refs.end())
      break;
    if (dst_ref->location >= equiv.dst_end())
      continue;
    // Straddle check.
    DCHECK_LE(dst_ref->location + ref_width, equiv.dst_end());

    offset_t src_loc =
        equiv.src_offset + (dst_ref->location - equiv.dst_offset);
    auto src_ref = std::lower_bound(
        src_refs.begin(), src_refs.end(), src_loc,
        [](const IndirectReference& a, offset_t b) { return a.location < b; });
    for (; dst_ref != dst_refs.end() &&
           dst_ref->location + ref_width <= equiv.dst_end();
         ++dst_ref, ++src_ref) {
      // Local offset of |src_ref| should match that of |dst_ref|.
      DCHECK_EQ(src_ref->location - equiv.src_offset,
                dst_ref->location - equiv.dst_offset);
      offset_t dst_index = new_label_manager.IndexOfOffset(
          dst_refs.target_pool().OffsetForKey(dst_ref->target_key));

      reference_delta_sink->PutNext(
          static_cast<int32_t>(dst_index - src_ref->target_key));
    }
    if (dst_ref == dst_refs.end())
      break;  // Done.
    // Straddle check.
    DCHECK_GE(dst_ref->location, equiv.dst_end());
  }
  return true;
}

bool GenerateExtraTargets(const std::vector<offset_t>& extra_targets,
                          PoolTag pool_tag,
                          PatchElementWriter* patch_writer) {
  TargetSink target_sink;
  for (offset_t target : extra_targets)
    target_sink.PutNext(target);
  patch_writer->SetTargetSink(pool_tag, std::move(target_sink));
  return true;
}

bool GenerateRawElement(const std::vector<offset_t>& old_sa,
                        ConstBufferView old_image,
                        ConstBufferView new_image,
                        PatchElementWriter* patch_writer) {
  ImageIndex old_image_index(old_image);
  ImageIndex new_image_index(new_image);

  EquivalenceMap equivalences;
  equivalences.Build(old_sa, EncodedView(old_image_index),
                     EncodedView(new_image_index), {},
                     kMinEquivalenceSimilarity);

  patch_writer->SetReferenceDeltaSink({});
  return GenerateEquivalencesAndExtraData(new_image, equivalences,
                                          patch_writer) &&
         GenerateRawDelta(old_image, new_image, equivalences, new_image_index,
                          patch_writer);
}

bool GenerateExecutableElement(ExecutableType exe_type,
                               ConstBufferView old_image,
                               ConstBufferView new_image,
                               PatchElementWriter* patch_writer) {
  // Initialize Disassemblers.
  std::unique_ptr<Disassembler> old_disasm =
      MakeDisassemblerOfType(old_image, exe_type);
  std::unique_ptr<Disassembler> new_disasm =
      MakeDisassemblerOfType(new_image, exe_type);
  if (!old_disasm || !new_disasm) {
    LOG(ERROR) << "Failed to create Disassembler.";
    return false;
  }
  DCHECK_EQ(old_disasm->GetExeType(), new_disasm->GetExeType());

  // Initialize ImageIndexes.
  ImageIndex old_image_index(old_image);
  ImageIndex new_image_index(new_image);
  if (!old_image_index.Initialize(old_disasm.get()) ||
      !new_image_index.Initialize(new_disasm.get())) {
    LOG(ERROR) << "Failed to create ImageIndex: Overlapping references found?";
    return false;
  }
  DCHECK_EQ(old_image_index.PoolCount(), new_image_index.PoolCount());
  size_t pool_count = old_image_index.PoolCount();

  EquivalenceMap equivalences =
      CreateEquivalenceMap(old_image_index, new_image_index);
  std::vector<Equivalence> forward_equivalences =
      equivalences.MakeForwardEquivalences();

  std::vector<UnorderedLabelManager> new_label_managers(pool_count);
  for (const auto& old_pool_tag_and_targets : old_image_index.target_pools()) {
    PoolTag pool_tag = old_pool_tag_and_targets.first;
    const auto& new_target_pool = new_image_index.pool(pool_tag);
    // Label Projection to initialize |new_label_manager|.
    std::vector<offset_t> new_labels = MakeNewTargetsFromEquivalenceMap(
        old_pool_tag_and_targets.second.targets(), forward_equivalences);
    new_label_managers[pool_tag.value()].Init(std::move(new_labels));

    // Find extra targets in |new_image_index|, emit into patch, merge them to
    // |new_labelsl_manager|, and update new references.
    OrderedLabelManager extra_label_manager;
    for (TypeTag type : new_target_pool.types()) {
      extra_label_manager.InsertOffsets(
          FindExtraTargets(new_image_index.refs(type),
                           new_label_managers[pool_tag.value()], equivalences));
    }
    if (!GenerateExtraTargets(extra_label_manager.Labels(), pool_tag,
                              patch_writer)) {
      return false;
    }
    for (offset_t offset : extra_label_manager.Labels())
      new_label_managers[pool_tag.value()].InsertNewOffset(offset);
  }
  ReferenceDeltaSink reference_delta_sink;
  for (const auto& old_refs : old_image_index.reference_sets()) {
    const auto& new_refs = new_image_index.refs(old_refs.first);
    if (!GenerateReferencesDelta(
            old_refs.second, new_refs,
            new_label_managers[new_refs.pool_tag().value()], equivalences,
            &reference_delta_sink)) {
      return false;
    }
  }
  patch_writer->SetReferenceDeltaSink(std::move(reference_delta_sink));
  return GenerateEquivalencesAndExtraData(new_image, equivalences,
                                          patch_writer) &&
         GenerateRawDelta(old_image, new_image, equivalences, new_image_index,
                          patch_writer);
}

/******** Exported Functions ********/

status::Code GenerateEnsemble(ConstBufferView old_image,
                              ConstBufferView new_image,
                              EnsemblePatchWriter* patch_writer) {
  std::unique_ptr<EnsembleMatcher> matcher =
      std::make_unique<HeuristicEnsembleMatcher>(nullptr);
  if (!matcher->RunMatch(old_image, new_image)) {
    LOG(INFO) << "RunMatch() failed, generating raw patch.";
    return GenerateRaw(old_image, new_image, patch_writer);
  }

  const std::vector<ElementMatch>& matches = matcher->matches();
  LOG(INFO) << "Matching: Found " << matches.size()
            << " nontrivial matches and " << matcher->num_identical()
            << " identical matches.";
  size_t num_elements = matches.size();
  if (num_elements == 0) {
    LOG(INFO) << "No nontrival matches, generating raw patch.";
    return GenerateRaw(old_image, new_image, patch_writer);
  }

  PatchType patch_type = PatchType::kRawPatch;
  if (num_elements == 1 && matches[0].old_element.size == old_image.size() &&
      matches[0].new_element.size == new_image.size()) {
    // If |old_image| matches |new_image| entirely then we have single patch.
    LOG(INFO) << "Old and new files are executables, "
              << "generating single-file patch.";
    patch_type = PatchType::kSinglePatch;
  } else {
    LOG(INFO) << "Generating ensemble patch.";
    patch_type = PatchType::kEnsemblePatch;
  }

  // "Gaps" are |new_image| bytes not covered by new_elements in |matches|.
  // These are treated as raw data, and patched against the entire |old_image|.

  // |patch_element_map| (keyed by "new" offsets) stores PatchElementWriter
  // results so elements and "gap" results can be computed separately (to reduce
  // peak memory usage), and later, properly serialized to |patch_writer|
  // ordered by "new" offset.
  std::map<offset_t, PatchElementWriter> patch_element_map;

  // Variables to track element patching successes.
  std::vector<BufferRegion> covered_new_regions;
  size_t covered_new_bytes = 0;

  // Process elements first, since non-fatal failures may turn some into gaps.
  for (const ElementMatch& match : matches) {
    BufferRegion new_region = match.new_element.region();
    LOG(INFO) << "--- Match [" << new_region.lo() << "," << new_region.hi()
              << ")";

    auto it_and_success = patch_element_map.emplace(
        base::checked_cast<offset_t>(new_region.lo()), match);
    DCHECK(it_and_success.second);
    PatchElementWriter& patch_element = it_and_success.first->second;

    ConstBufferView old_sub_image = old_image[match.old_element.region()];
    ConstBufferView new_sub_image = new_image[new_region];
    if (GenerateExecutableElement(match.exe_type(), old_sub_image,
                                  new_sub_image, &patch_element)) {
      covered_new_regions.push_back(new_region);
      covered_new_bytes += new_region.size;
    } else {
      LOG(INFO) << "Fall back to raw patching.";
      patch_element_map.erase(it_and_success.first);
    }
  }

  if (covered_new_bytes == 0)
    patch_type = PatchType::kRawPatch;

  if (covered_new_bytes < new_image.size()) {
    // Process all "gaps", which are patched against the entire "old" image. To
    // compute equivalence maps, "gaps" share a common suffix array
    // |old_sa_raw|, whose lifetime is kept separated from elements' suffix
    // arrays to reduce peak memory.
    Element entire_old_element(old_image.local_region(), kExeTypeNoOp);
    ImageIndex old_image_index(old_image);
    EncodedView old_view_raw(old_image_index);
    std::vector<offset_t> old_sa_raw =
        MakeSuffixArray<InducedSuffixSort>(old_view_raw, size_t(256));

    offset_t gap_lo = 0;
    // Add sentinel that points to end of "new" file, to simplify gap iteration.
    covered_new_regions.emplace_back(BufferRegion{new_image.size(), 0});

    for (const BufferRegion& covered : covered_new_regions) {
      offset_t gap_hi = base::checked_cast<offset_t>(covered.lo());
      DCHECK_GE(gap_hi, gap_lo);
      offset_t gap_size = gap_hi - gap_lo;
      if (gap_size > 0) {
        LOG(INFO) << "--- Gap   [" << gap_lo << "," << gap_hi << ")";

        ElementMatch gap_match{{entire_old_element, kExeTypeNoOp},
                               {{gap_lo, gap_size}, kExeTypeNoOp}};
        auto it_and_success = patch_element_map.emplace(gap_lo, gap_match);
        DCHECK(it_and_success.second);
        PatchElementWriter& patch_element = it_and_success.first->second;

        ConstBufferView new_sub_image = new_image[{gap_lo, gap_size}];
        if (!GenerateRawElement(old_sa_raw, old_image, new_sub_image,
                                &patch_element)) {
          return status::kStatusFatal;
        }
      }
      gap_lo = base::checked_cast<offset_t>(covered.hi());
    }
  }

  patch_writer->SetPatchType(patch_type);
  // Write all PatchElementWriter sorted by "new" offset.
  for (auto& new_lo_and_patch_element : patch_element_map)
    patch_writer->AddElement(std::move(new_lo_and_patch_element.second));

  return status::kStatusSuccess;
}

status::Code GenerateRaw(ConstBufferView old_image,
                         ConstBufferView new_image,
                         EnsemblePatchWriter* patch_writer) {
  patch_writer->SetPatchType(PatchType::kRawPatch);

  ImageIndex old_image_index(old_image);
  EncodedView old_view(old_image_index);
  std::vector<offset_t> old_sa =
      MakeSuffixArray<InducedSuffixSort>(old_view, old_view.Cardinality());

  PatchElementWriter patch_element(
      {Element(old_image.local_region()), Element(new_image.local_region())});
  if (!GenerateRawElement(old_sa, old_image, new_image, &patch_element))
    return status::kStatusFatal;
  patch_writer->AddElement(std::move(patch_element));
  return status::kStatusSuccess;
}

}  // namespace zucchini
