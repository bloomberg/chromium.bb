// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/image_index.h"

#include <algorithm>
#include <utility>

#include "chrome/installer/zucchini/disassembler.h"

namespace zucchini {

ImageIndex::ImageIndex(ConstBufferView image)
    : image_(image), type_tags_(image.size(), kNoTypeTag) {}

ImageIndex::ImageIndex(ImageIndex&& that) = default;

ImageIndex::~ImageIndex() = default;

bool ImageIndex::Initialize(Disassembler* disasm) {
  std::vector<ReferenceGroup> ref_groups = disasm->MakeReferenceGroups();

  for (const auto& group : ref_groups) {
    // Store TypeInfo for current type (of |group|).
    DCHECK_NE(kNoTypeTag, group.type_tag());
    auto result = types_.emplace(group.type_tag(), group.traits());
    DCHECK(result.second);

    // Find and store all references for current type, returns false on finding
    // any overlap, to signal error.
    if (!InsertReferences(group.type_tag(),
                          std::move(*group.GetReader(disasm)))) {
      return false;
    }

    // Build pool-to-type mapping.
    DCHECK_NE(kNoPoolTag, group.pool_tag());
    pools_[group.pool_tag()].types.push_back(group.type_tag());
  }
  return true;
}

Reference ImageIndex::FindReference(TypeTag type, offset_t location) const {
  DCHECK_LE(location, size());
  DCHECK_LT(type.value(), types_.size());
  const TypeInfo& type_info = types_.at(type);
  auto pos = std::upper_bound(
      type_info.references.begin(), type_info.references.end(), location,
      [](offset_t a, const Reference& ref) { return a < ref.location; });

  DCHECK(pos != type_info.references.begin());
  --pos;
  DCHECK_LT(location, pos->location + GetTraits(type).width);
  return *pos;
}

std::vector<offset_t> ImageIndex::GetTargets(PoolTag pool) const {
  size_t target_count = 0;
  for (TypeTag type : GetTypeTags(pool))
    target_count += GetReferences(type).size();
  std::vector<offset_t> targets;
  targets.reserve(target_count);
  for (TypeTag type : GetTypeTags(pool))
    for (const auto& ref : GetReferences(type))
      targets.push_back(ref.target);
  return targets;
}

bool ImageIndex::IsToken(offset_t location) const {
  TypeTag type = LookupType(location);

  // |location| points into raw data.
  if (type == kNoTypeTag)
    return true;

  // |location| points into a Reference.
  Reference reference = FindReference(type, location);
  // Only the first byte of a reference is a token.
  return location == reference.location;
}

void ImageIndex::LabelTargets(PoolTag pool,
                              const BaseLabelManager& label_manager) {
  for (const TypeTag& type : pools_.at(pool).types)
    for (auto& ref : types_.at(type).references)
      ref.target = label_manager.MarkedIndexFromOffset(ref.target);
  pools_.at(pool).label_bound = label_manager.size();
}

void ImageIndex::UnlabelTargets(PoolTag pool,
                                const BaseLabelManager& label_manager) {
  for (const TypeTag& type : pools_.at(pool).types)
    for (auto& ref : types_.at(type).references) {
      ref.target = label_manager.OffsetFromMarkedIndex(ref.target);
      DCHECK(!IsMarked(ref.target));  // Expected to be represented as offset.
    }
  pools_.at(pool).label_bound = 0;
}

void ImageIndex::LabelAssociatedTargets(
    PoolTag pool,
    const BaseLabelManager& label_manager,
    const BaseLabelManager& reference_label_manager) {
  // Convert to marked indexes.
  for (const auto& type : pools_.at(pool).types) {
    for (auto& ref : types_.at(type).references) {
      // Represent Label as marked index iff the index is also in
      // |reference_label_manager|.
      DCHECK(!IsMarked(ref.target));  // Expected to be represented as offset.
      offset_t index = label_manager.IndexOfOffset(ref.target);
      DCHECK_NE(kUnusedIndex, index);  // Target is expected to have a label.
      if (reference_label_manager.IsIndexStored(index))
        ref.target = MarkIndex(index);
    }
  }
  pools_.at(pool).label_bound = label_manager.size();
}

bool ImageIndex::InsertReferences(TypeTag type, ReferenceReader&& ref_reader) {
  const ReferenceTypeTraits& traits = GetTraits(type);
  TypeInfo& type_info = types_.at(traits.type_tag);
  for (base::Optional<Reference> ref = ref_reader.GetNext(); ref.has_value();
       ref = ref_reader.GetNext()) {
    DCHECK_LE(ref->location + traits.width, size());
    auto cur_type_tag = type_tags_.begin() + ref->location;

    // Check for overlap with existing reference. If found, then invalidate.
    if (std::any_of(cur_type_tag, cur_type_tag + traits.width,
                    [](TypeTag type) { return type != kNoTypeTag; })) {
      return false;
    }
    std::fill(cur_type_tag, cur_type_tag + traits.width, traits.type_tag);
    type_info.references.push_back(*ref);
  }
  DCHECK(std::is_sorted(type_info.references.begin(),
                        type_info.references.end(),
                        [](const Reference& a, const Reference& b) {
                          return a.location < b.location;
                        }));
  return true;
}

ImageIndex::TypeInfo::TypeInfo(ReferenceTypeTraits traits_in)
    : traits(traits_in) {}
ImageIndex::TypeInfo::TypeInfo(TypeInfo&&) = default;
ImageIndex::TypeInfo::~TypeInfo() = default;

ImageIndex::PoolInfo::PoolInfo() = default;
ImageIndex::PoolInfo::PoolInfo(PoolInfo&&) = default;
ImageIndex::PoolInfo::~PoolInfo() = default;

}  // namespace zucchini
