// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/patch_reader.h"

#include <type_traits>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "chrome/installer/zucchini/crc32.h"

namespace zucchini {

namespace patch {

bool ParseElementMatch(BufferSource* source, ElementMatch* element_match) {
  PatchElementHeader element_header;
  if (!source->GetValue(&element_header)) {
    LOG(ERROR) << "Impossible to read ElementMatch from source.";
    LOG(ERROR) << base::debug::StackTrace().ToString();
    return false;
  }
  ExecutableType exe_type =
      static_cast<ExecutableType>(element_header.exe_type);
  if (exe_type >= kNumExeType) {
    LOG(ERROR) << "Invalid ExecutableType encountered.";
    LOG(ERROR) << base::debug::StackTrace().ToString();
    return false;
  }
  element_match->old_element.offset = element_header.old_offset;
  element_match->new_element.offset = element_header.new_offset;
  element_match->old_element.size = element_header.old_length;
  element_match->new_element.size = element_header.new_length;
  element_match->old_element.exe_type = exe_type;
  element_match->new_element.exe_type = exe_type;
  return true;
}

bool ParseBuffer(BufferSource* source, BufferSource* buffer) {
  uint32_t size = 0;
  if (!source->GetValue(&size)) {
    LOG(ERROR) << "Impossible to read buffer size from source.";
    LOG(ERROR) << base::debug::StackTrace().ToString();
    return false;
  }
  if (!source->GetRegion(base::checked_cast<size_t>(size), buffer)) {
    LOG(ERROR) << "Impossible to read buffer content from source.";
    LOG(ERROR) << base::debug::StackTrace().ToString();
    return false;
  }
  return true;
}

}  // namespace patch

/******** EquivalenceSource ********/

EquivalenceSource::EquivalenceSource() = default;
EquivalenceSource::EquivalenceSource(const EquivalenceSource&) = default;
EquivalenceSource::~EquivalenceSource() = default;

bool EquivalenceSource::Initialize(BufferSource* source) {
  return patch::ParseBuffer(source, &src_skip_) &&
         patch::ParseBuffer(source, &dst_skip_) &&
         patch::ParseBuffer(source, &copy_count_);
}

base::Optional<Equivalence> EquivalenceSource::GetNext() {
  if (src_skip_.empty() || dst_skip_.empty() || copy_count_.empty())
    return base::nullopt;

  Equivalence equivalence = {};

  uint32_t length = 0;
  if (!patch::ParseVarUInt<uint32_t>(&copy_count_, &length))
    return base::nullopt;
  equivalence.length = base::strict_cast<offset_t>(length);

  int32_t src_offset_diff = 0;  // Intentionally signed.
  if (!patch::ParseVarInt<int32_t>(&src_skip_, &src_offset_diff))
    return base::nullopt;
  base::CheckedNumeric<offset_t> src_offset =
      previous_src_offset_ + src_offset_diff;
  if (!src_offset.IsValid())
    return base::nullopt;

  equivalence.src_offset = src_offset.ValueOrDie();
  previous_src_offset_ = src_offset + equivalence.length;
  if (!previous_src_offset_.IsValid())
    return base::nullopt;

  uint32_t dst_offset_diff = 0;  // Intentionally unsigned.
  if (!patch::ParseVarUInt<uint32_t>(&dst_skip_, &dst_offset_diff))
    return base::nullopt;
  base::CheckedNumeric<offset_t> dst_offset =
      previous_dst_offset_ + dst_offset_diff;
  if (!dst_offset.IsValid())
    return base::nullopt;

  equivalence.dst_offset = dst_offset.ValueOrDie();
  previous_dst_offset_ = equivalence.dst_offset + equivalence.length;
  if (!previous_dst_offset_.IsValid())
    return base::nullopt;

  return equivalence;
}

/******** ExtraDataSource ********/

ExtraDataSource::ExtraDataSource() = default;
ExtraDataSource::ExtraDataSource(const ExtraDataSource&) = default;
ExtraDataSource::~ExtraDataSource() = default;

bool ExtraDataSource::Initialize(BufferSource* source) {
  return patch::ParseBuffer(source, &extra_data_);
}

base::Optional<ConstBufferView> ExtraDataSource::GetNext(offset_t size) {
  ConstBufferView buffer;
  if (!extra_data_.GetRegion(size, &buffer))
    return base::nullopt;
  return buffer;
}

/******** RawDeltaSource ********/

RawDeltaSource::RawDeltaSource() = default;
RawDeltaSource::RawDeltaSource(const RawDeltaSource&) = default;
RawDeltaSource::~RawDeltaSource() = default;

bool RawDeltaSource::Initialize(BufferSource* source) {
  return patch::ParseBuffer(source, &raw_delta_skip_) &&
         patch::ParseBuffer(source, &raw_delta_diff_);
}

base::Optional<RawDeltaUnit> RawDeltaSource::GetNext() {
  if (raw_delta_skip_.empty() || raw_delta_diff_.empty())
    return base::nullopt;

  RawDeltaUnit delta = {};
  uint32_t copy_offset_diff = 0;
  if (!patch::ParseVarUInt<uint32_t>(&raw_delta_skip_, &copy_offset_diff))
    return base::nullopt;
  base::CheckedNumeric<offset_t> copy_offset =
      copy_offset_diff + copy_offset_compensation_;
  if (!copy_offset.IsValid())
    return base::nullopt;
  delta.copy_offset = copy_offset.ValueOrDie();

  if (!raw_delta_diff_.GetValue<int8_t>(&delta.diff))
    return base::nullopt;

  // We keep track of the compensation needed for next offset, taking into
  // accound delta encoding and bias of -1.
  copy_offset_compensation_ = copy_offset + 1;
  if (!copy_offset_compensation_.IsValid())
    return base::nullopt;
  return delta;
}

/******** ReferenceDeltaSource ********/

ReferenceDeltaSource::ReferenceDeltaSource() = default;
ReferenceDeltaSource::ReferenceDeltaSource(const ReferenceDeltaSource&) =
    default;
ReferenceDeltaSource::~ReferenceDeltaSource() = default;

bool ReferenceDeltaSource::Initialize(BufferSource* source) {
  return patch::ParseBuffer(source, &reference_delta_);
}

base::Optional<int32_t> ReferenceDeltaSource::GetNext() {
  if (reference_delta_.empty())
    return base::nullopt;
  int32_t delta = 0;
  if (!patch::ParseVarInt<int32_t>(&reference_delta_, &delta))
    return base::nullopt;
  return delta;
}

/******** TargetSource ********/

TargetSource::TargetSource() = default;
TargetSource::TargetSource(const TargetSource&) = default;
TargetSource::~TargetSource() = default;

bool TargetSource::Initialize(BufferSource* source) {
  return patch::ParseBuffer(source, &extra_targets_);
}

base::Optional<offset_t> TargetSource::GetNext() {
  if (extra_targets_.empty())
    return base::nullopt;

  uint32_t target_diff = 0;
  if (!patch::ParseVarUInt<uint32_t>(&extra_targets_, &target_diff))
    return base::nullopt;
  base::CheckedNumeric<offset_t> target = target_diff + target_compensation_;
  if (!target.IsValid())
    return base::nullopt;

  // We keep track of the compensation needed for next target, taking into
  // accound delta encoding and bias of -1.
  target_compensation_ = target + 1;
  if (!target_compensation_.IsValid())
    return base::nullopt;
  return offset_t(target.ValueOrDie());
}

/******** PatchElementReader ********/

PatchElementReader::PatchElementReader() = default;
PatchElementReader::PatchElementReader(PatchElementReader&&) = default;
PatchElementReader::~PatchElementReader() = default;

bool PatchElementReader::Initialize(BufferSource* source) {
  bool ok = patch::ParseElementMatch(source, &element_match_) &&
            equivalences_.Initialize(source) &&
            extra_data_.Initialize(source) && raw_delta_.Initialize(source) &&
            reference_delta_.Initialize(source);
  if (!ok)
    return false;
  uint32_t pool_count = 0;
  if (!source->GetValue(&pool_count)) {
    LOG(ERROR) << "Impossible to read pool_count from source.";
    return false;
  }
  for (uint32_t i = 0; i < pool_count; ++i) {
    uint8_t pool_tag_value = 0;
    if (!source->GetValue(&pool_tag_value)) {
      LOG(ERROR) << "Impossible to read pool_tag from source.";
      return false;
    }
    PoolTag pool_tag(pool_tag_value);
    if (pool_tag == kNoPoolTag) {
      LOG(ERROR) << "Invalid pool_tag encountered in ExtraTargetList.";
      return false;
    }
    auto insert_result = extra_targets_.insert({pool_tag, {}});
    if (!insert_result.second) {  // Element already present.
      LOG(ERROR) << "Multiple ExtraTargetList found for the same pool_tag";
      return false;
    }
    if (!insert_result.first->second.Initialize(source))
      return false;
  }
  return true;
}

/******** EnsemblePatchReader ********/

base::Optional<EnsemblePatchReader> EnsemblePatchReader::Create(
    ConstBufferView buffer) {
  BufferSource source(buffer);
  EnsemblePatchReader patch;
  if (!patch.Initialize(&source))
    return base::nullopt;
  return patch;
}

EnsemblePatchReader::EnsemblePatchReader() = default;
EnsemblePatchReader::EnsemblePatchReader(EnsemblePatchReader&&) = default;
EnsemblePatchReader::~EnsemblePatchReader() = default;

bool EnsemblePatchReader::Initialize(BufferSource* source) {
  if (!source->GetValue(&header_)) {
    LOG(ERROR) << "Impossible to read header from source.";
    return false;
  }
  if (header_.magic != PatchHeader::kMagic) {
    LOG(ERROR) << "Patch contains invalid magic.";
    return false;
  }
  uint32_t patch_type_int =
      static_cast<uint32_t>(PatchType::kUnrecognisedPatch);
  if (!source->GetValue(&patch_type_int)) {
    LOG(ERROR) << "Impossible to read patch_type from source.";
    return false;
  }
  patch_type_ = static_cast<PatchType>(patch_type_int);
  if (patch_type_ != PatchType::kRawPatch &&
      patch_type_ != PatchType::kSinglePatch &&
      patch_type_ != PatchType::kEnsemblePatch) {
    LOG(ERROR) << "Invalid patch_type encountered.";
    return false;
  }

  uint32_t element_count = 0;
  if (!source->GetValue(&element_count)) {
    LOG(ERROR) << "Impossible to read element_count from source.";
    return false;
  }
  if (patch_type_ == PatchType::kRawPatch ||
      patch_type_ == PatchType::kSinglePatch) {
    if (element_count != 1) {
      LOG(ERROR) << "Unexpected number of elements in patch.";
      return false;  // Only one element expected.
    }
  }

  offset_t current_dst_offset = 0;
  for (uint32_t i = 0; i < element_count; ++i) {
    PatchElementReader element_patch;
    if (!element_patch.Initialize(source))
      return false;

    if (!element_patch.old_element().FitsIn(header_.old_size) ||
        !element_patch.new_element().FitsIn(header_.new_size)) {
      LOG(ERROR) << "Invalid element encountered.";
      return false;
    }

    if (element_patch.new_element().offset != current_dst_offset) {
      LOG(ERROR) << "Invalid element encountered.";
      return false;
    }
    current_dst_offset = element_patch.new_element().EndOffset();

    elements_.push_back(std::move(element_patch));
  }
  if (current_dst_offset != header_.new_size) {
    LOG(ERROR) << "Patch elements don't fully cover new image file.";
    return false;
  }

  if (!source->empty()) {
    LOG(ERROR) << "Patch was not fully consumed.";
    return false;
  }

  return true;
}

bool EnsemblePatchReader::CheckOldFile(ConstBufferView old_image) const {
  return old_image.size() == header_.old_size &&
         CalculateCrc32(old_image.begin(), old_image.end()) == header_.old_crc;
}

bool EnsemblePatchReader::CheckNewFile(ConstBufferView new_image) const {
  return new_image.size() == header_.new_size &&
         CalculateCrc32(new_image.begin(), new_image.end()) == header_.new_crc;
}

}  // namespace zucchini
