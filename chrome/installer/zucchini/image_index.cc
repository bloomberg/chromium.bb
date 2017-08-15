// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/image_index.h"

#include <algorithm>

namespace zucchini {

ImageIndex::ImageIndex(ConstBufferView image,
                       std::vector<ReferenceTypeTraits>&& traits_map)
    : raw_image_(image),
      type_tags_(image.size(), kNoTypeTag),
      types_(traits_map.size()) {
  int pool_count = 0;
  for (const auto& traits : traits_map) {
    DCHECK_LT(traits.type_tag.value(), types_.size());
    types_[traits.type_tag.value()].traits = traits;

    DCHECK(kNoPoolTag != traits.pool_tag);
    pool_count = std::max(pool_count, traits.pool_tag.value() + 1);
  }
  pools_.resize(pool_count);
  for (const auto& traits : traits_map) {
    pools_[traits.pool_tag.value()].types.push_back(traits.type_tag);
  }
}

ImageIndex::ImageIndex(ImageIndex&& that) = default;

ImageIndex::~ImageIndex() = default;

bool ImageIndex::InsertReferences(TypeTag type, ReferenceReader&& ref_reader) {
  const ReferenceTypeTraits& traits = GetTraits(type);
  for (base::Optional<Reference> ref = ref_reader.GetNext(); ref.has_value();
       ref = ref_reader.GetNext()) {
    DCHECK_LE(ref->location + traits.width, size());

    // Check for overlap with existing reference. If found, then invalidate.
    if (std::any_of(type_tags_.begin() + ref->location,
                    type_tags_.begin() + ref->location + traits.width,
                    [](TypeTag type) { return type != kNoTypeTag; }))
      return false;
    std::fill(type_tags_.begin() + ref->location,
              type_tags_.begin() + ref->location + traits.width,
              traits.type_tag);
    types_[traits.type_tag.value()].references.push_back(*ref);
  }
  return true;
}

Reference ImageIndex::FindReference(TypeTag type, offset_t location) const {
  DCHECK_LE(location, size());
  DCHECK_LT(type.value(), types_.size());
  auto pos = std::upper_bound(
      types_[type.value()].references.begin(),
      types_[type.value()].references.end(), location,
      [](offset_t a, const Reference& ref) { return a < ref.location; });

  DCHECK(pos != types_[type.value()].references.begin());
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
  TypeTag type = GetType(location);

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
  for (const TypeTag& type : pools_[pool.value()].types)
    for (auto& ref : types_[type.value()].references)
      ref.target = label_manager.MarkedIndexFromOffset(ref.target);
  pools_[pool.value()].label_bound = label_manager.size();
}

void ImageIndex::UnlabelTargets(PoolTag pool,
                                const BaseLabelManager& label_manager) {
  for (const TypeTag& type : pools_[pool.value()].types)
    for (auto& ref : types_[type.value()].references) {
      ref.target = label_manager.OffsetFromMarkedIndex(ref.target);
      DCHECK(!IsMarked(ref.target));  // Expected to be represented as offset.
    }
  pools_[pool.value()].label_bound = 0;
}

void ImageIndex::LabelAssociatedTargets(
    PoolTag pool,
    const BaseLabelManager& label_manager,
    const BaseLabelManager& reference_label_manager) {
  // Convert to marked indexes.
  for (const auto& type : pools_[pool.value()].types) {
    for (auto& ref : types_[type.value()].references) {
      // Represent Label as marked index iff the index is also in
      // |reference_label_manager|.
      DCHECK(!IsMarked(ref.target));  // Expected to be represented as offset.
      offset_t index = label_manager.IndexOfOffset(ref.target);
      DCHECK_NE(kUnusedIndex, index);  // Target is expected to have a label.
      if (reference_label_manager.IsIndexStored(index))
        ref.target = MarkIndex(index);
    }
  }
  pools_[pool.value()].label_bound = label_manager.size();
}

ImageIndex::TypeInfo::TypeInfo() = default;
ImageIndex::TypeInfo::TypeInfo(TypeInfo&&) = default;
ImageIndex::TypeInfo::~TypeInfo() = default;

ImageIndex::PoolInfo::PoolInfo() = default;
ImageIndex::PoolInfo::PoolInfo(PoolInfo&&) = default;
ImageIndex::PoolInfo::~PoolInfo() = default;

}  // namespace zucchini
