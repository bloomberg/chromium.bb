// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/target_pool.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/logging.h"
#include "chrome/installer/zucchini/algorithm.h"

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

}  // namespace zucchini
