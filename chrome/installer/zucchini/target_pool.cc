// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/target_pool.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/logging.h"
#include "chrome/installer/zucchini/algorithm.h"
#include "chrome/installer/zucchini/label_manager.h"

namespace zucchini {

TargetPool::TargetPool() = default;

TargetPool::TargetPool(std::vector<offset_t>&& targets) {
  DCHECK(targets_.empty());
  DCHECK(std::is_sorted(targets.begin(), targets.end()));
  targets_ = std::move(targets);
}

TargetPool::TargetPool(TargetPool&&) = default;
TargetPool::~TargetPool() = default;

void TargetPool::InsertTargets(const std::vector<Reference>& references) {
  // This can be called many times, so it's better to let std::back_inserter()
  // manage |targets_| resize, instead of manually reserving space.
  std::transform(references.begin(), references.end(),
                 std::back_inserter(targets_),
                 [](const Reference& ref) { return ref.target; });
  SortAndUniquify(&targets_);
}

void TargetPool::InsertTargets(ReferenceReader&& references) {
  for (auto ref = references.GetNext(); ref.has_value();
       ref = references.GetNext()) {
    targets_.push_back(ref->target);
  }
  SortAndUniquify(&targets_);
}

offset_t TargetPool::KeyForOffset(offset_t offset) const {
  auto pos = std::lower_bound(targets_.begin(), targets_.end(), offset);
  DCHECK(pos != targets_.end() && *pos == offset);
  return static_cast<offset_t>(pos - targets_.begin());
}

void TargetPool::LabelTargets(const BaseLabelManager& label_manager) {
  for (auto& target : targets_)
    target = label_manager.MarkedIndexFromOffset(target);
  label_bound_ = label_manager.size();
}

void TargetPool::UnlabelTargets(const BaseLabelManager& label_manager) {
  for (auto& target : targets_) {
    target = label_manager.OffsetFromMarkedIndex(target);
    DCHECK(!IsMarked(target));  // Expected to be represented as offset.
  }
  label_bound_ = 0;
}

void TargetPool::LabelAssociatedTargets(
    const BaseLabelManager& label_manager,
    const BaseLabelManager& reference_label_manager) {
  // Convert to marked indexes.
  for (auto& target : targets_) {
    // Represent Label as marked index iff the index is also in
    // |reference_label_manager|.
    DCHECK(!IsMarked(target));  // Expected to be represented as offset.
    offset_t index = label_manager.IndexOfOffset(target);
    DCHECK_NE(kUnusedIndex, index);  // Target is expected to have a label.
    if (reference_label_manager.IsIndexStored(index))
      target = MarkIndex(index);
  }
  label_bound_ = label_manager.size();
}

}  // namespace zucchini
