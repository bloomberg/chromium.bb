// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/label_manager.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"

namespace zucchini {

namespace {

void SortAndUniquify(std::vector<offset_t>* offsets) {
  std::sort(offsets->begin(), offsets->end());
  offsets->erase(std::unique(offsets->begin(), offsets->end()), offsets->end());
  offsets->shrink_to_fit();
}

}  // namespace

/******** BaseLabelManager ********/

BaseLabelManager::BaseLabelManager() = default;
BaseLabelManager::BaseLabelManager(const BaseLabelManager&) = default;
BaseLabelManager::~BaseLabelManager() = default;

offset_t BaseLabelManager::OffsetFromMarkedIndex(
    offset_t offset_or_marked_index) const {
  if (IsMarkedIndex(offset_or_marked_index)) {
    offset_t offset = OffsetOfIndex(UnmarkIndex(offset_or_marked_index));
    if (offset != kUnusedIndex)
      return offset;
  }
  return offset_or_marked_index;
}

/******** OrderedLabelManager ********/

OrderedLabelManager::OrderedLabelManager() = default;
OrderedLabelManager::OrderedLabelManager(const OrderedLabelManager&) = default;
OrderedLabelManager::~OrderedLabelManager() = default;

offset_t OrderedLabelManager::IndexOfOffset(offset_t offset) const {
  auto it = std::lower_bound(labels_.begin(), labels_.end(), offset);
  if (it != labels_.end() && *it == offset)
    return static_cast<offset_t>(it - labels_.begin());
  return kUnusedIndex;
}

offset_t OrderedLabelManager::MarkedIndexFromOffset(
    offset_t offset_or_marked_index) const {
  if (IsOffset(offset_or_marked_index)) {
    auto it = std::lower_bound(labels_.begin(), labels_.end(),
                               offset_or_marked_index);
    if (it != labels_.end() && *it == offset_or_marked_index)
      return MarkIndex(static_cast<offset_t>(it - labels_.begin()));
  }
  return offset_or_marked_index;
}

void OrderedLabelManager::InsertOffsets(const std::vector<offset_t>& offsets) {
  labels_.insert(labels_.end(), offsets.begin(), offsets.end());
  SortAndUniquify(&labels_);
}

void OrderedLabelManager::InsertTargets(ReferenceReader&& reader) {
  for (auto ref = reader.GetNext(); ref.has_value(); ref = reader.GetNext())
    labels_.push_back(ref->target);
  SortAndUniquify(&labels_);
}

/******** UnorderedLabelManager ********/

UnorderedLabelManager::UnorderedLabelManager() = default;
UnorderedLabelManager::UnorderedLabelManager(const UnorderedLabelManager&) =
    default;
UnorderedLabelManager::~UnorderedLabelManager() = default;

offset_t UnorderedLabelManager::IndexOfOffset(offset_t offset) const {
  auto it = labels_map_.find(offset);
  return it != labels_map_.end() ? it->second : kUnusedIndex;
}

offset_t UnorderedLabelManager::MarkedIndexFromOffset(
    offset_t offset_or_marked_index) const {
  if (IsOffset(offset_or_marked_index)) {
    auto it = labels_map_.find(offset_or_marked_index);
    if (it != labels_map_.end())
      return MarkIndex(it->second);
  }
  return offset_or_marked_index;
}

void UnorderedLabelManager::Init(std::vector<offset_t>&& labels) {
  labels_ = std::move(labels);
  labels_map_.clear();
  gap_idx_ = 0;

  size_t used_index_count = 0;
  for (offset_t label : labels) {
    if (label != kUnusedIndex)
      ++used_index_count;
  }
  labels_map_.reserve(used_index_count);

  offset_t size = static_cast<offset_t>(labels_.size());
  for (offset_t idx = 0; idx < size; ++idx) {
    if (labels_[idx] != kUnusedIndex) {
      DCHECK(labels_map_.find(labels_[idx]) == labels_map_.end());
      labels_map_[labels_[idx]] = idx;
    }
  }
}

void UnorderedLabelManager::InsertNewOffset(offset_t offset) {
  DCHECK(labels_map_.find(offset) == labels_map_.end());
  // Look for unused entry in |labels_|.
  auto pos = std::find(labels_.begin() + gap_idx_, labels_.end(), kUnusedIndex);
  // Either replace the unused entry, or insert at end.
  if (pos != labels_.end()) {
    gap_idx_ = pos - labels_.begin();
    *pos = offset;
  } else {
    gap_idx_ = labels_.size();
    labels_.push_back(offset);
  }
  labels_map_[offset] = static_cast<offset_t>(gap_idx_);
}

}  // namespace zucchini
