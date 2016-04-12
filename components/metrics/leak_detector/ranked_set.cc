// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/ranked_set.h"

#include <algorithm>

namespace metrics {
namespace leak_detector {

RankedSet::RankedSet(size_t max_size) : max_size_(max_size) {}

RankedSet::~RankedSet() {}

RankedSet::RankedSet(RankedSet&& other) : max_size_(other.max_size_) {
  entries_ = std::move(other.entries_);
}

RankedSet& RankedSet::operator=(RankedSet&& other) {
  max_size_ = other.max_size_;
  entries_ = std::move(other.entries_);
  return *this;
}

bool RankedSet::Entry::operator<(const RankedSet::Entry& other) const {
  if (count == other.count)
    return value < other.value;

  return count > other.count;
}

void RankedSet::Add(const ValueType& value, int count) {
  // If the container is full, do not add any entry with |count| if does not
  // exceed the lowest count of the entries in the list.
  if (size() == max_size_ && count < min_count())
    return;

  Entry new_entry;
  new_entry.value = value;
  new_entry.count = count;
  entries_.insert(new_entry);

  // Limit the container size if it exceeds the maximum allowed size, by
  // deleting the last element. This should only iterate once because the size
  // can only have increased by 1, but use a while loop just to be safe.
  while (entries_.size() > max_size_)
    entries_.erase(--entries_.end());
}

}  // namespace leak_detector
}  // namespace metrics
