// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/ranked_list.h"

#include <algorithm>

namespace metrics {
namespace leak_detector {

RankedList::RankedList(size_t max_size) : max_size_(max_size) {}

RankedList::~RankedList() {}

RankedList::RankedList(RankedList&& other)
    : max_size_(other.max_size_) {
  entries_ = std::move(other.entries_);
}

RankedList& RankedList::operator=(RankedList&& other) {
  max_size_ = other.max_size_;
  entries_ = std::move(other.entries_);
  return *this;
}

void RankedList::Add(const ValueType& value, int count) {
  Entry new_entry;
  new_entry.value = value;
  new_entry.count = count;

  // Determine where to insert the value given its count.
  EntryList::iterator iter = std::upper_bound(entries_.begin(), entries_.end(),
                                              new_entry);

  // If the list is full, do not add any entry with |count| if does not exceed
  // the lowest count of the entries in the list.
  if (size() == max_size_ && iter == end())
    return;

  entries_.insert(iter, new_entry);

  // Limit the list size if it exceeds the maximum allowed size.
  if (entries_.size() > max_size_)
    entries_.resize(max_size_);
}

}  // namespace leak_detector
}  // namespace metrics
