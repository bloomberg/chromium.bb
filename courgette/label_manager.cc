// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/label_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "courgette/consecutive_range_visitor.h"

namespace courgette {

LabelManager::RvaVisitor::~RvaVisitor() {}

LabelManager::LabelManager() {}

LabelManager::~LabelManager() {}

// We wish to minimize peak memory usage here. Analysis: Let
//   m = number of (RVA) elements in |rva_visitor|,
//   n = number of distinct (RVA) elements in |rva_visitor|.
// The final storage is n * sizeof(Label) bytes. During computation we uniquify
// m RVAs, and count repeats. Taking sizeof(RVA) = 4, an implementation using
// std::map or std::unordered_map would consume additionally 32 * n bytes.
// Meanwhile, our std::vector implementation consumes additionally 4 * m bytes
// For our typical usage (i.e. Chrome) we see m = ~4n, so we use 16 * n bytes of
// extra contiguous memory during computation. Assuming memory fragmentation
// would not be an issue, this is much better than using std::map.
void LabelManager::Read(RvaVisitor* rva_visitor) {
  // Write all values in |rva_visitor| to |rvas|.
  size_t num_rva = rva_visitor->Remaining();
  std::vector<RVA> rvas(num_rva);
  for (size_t i = 0; i < num_rva; ++i, rva_visitor->Next())
    rvas[i] = rva_visitor->Get();

  // Sort |rvas|, then count the number of distinct values.
  using CRV = ConsecutiveRangeVisitor<std::vector<RVA>::iterator>;
  std::sort(rvas.begin(), rvas.end());
  size_t num_distinct_rva = 0;
  for (CRV it(rvas.begin(), rvas.end()); it.has_more(); it.advance())
    ++num_distinct_rva;

  // Reserve space for |labels_|, populate with sorted RVA and repeats.
  DCHECK(labels_.empty());
  labels_.reserve(num_distinct_rva);
  for (CRV it(rvas.begin(), rvas.end()); it.has_more(); it.advance()) {
    labels_.push_back(Label(*it.cur()));
    base::CheckedNumeric<uint32> count = it.repeat();
    labels_.back().count_ = count.ValueOrDie();
  }
}

void LabelManager::RemoveUnderusedLabels(int32 count_threshold) {
  if (count_threshold <= 0)
    return;
  labels_.erase(std::remove_if(labels_.begin(), labels_.end(),
                               [count_threshold](const Label& label) {
                                 return label.count_ < count_threshold;
                               }),
                labels_.end());
  // Not shrinking |labels_|, since this may cause reallocation.
}

// Uses binary search to find |rva|.
Label* LabelManager::Find(RVA rva) {
  auto it = std::lower_bound(
      labels_.begin(), labels_.end(), Label(rva),
      [](const Label& l1, const Label& l2) { return l1.rva_ < l2.rva_; });
  return it == labels_.end() || it->rva_ != rva ? nullptr : &(*it);
}

}  // namespace courgette
