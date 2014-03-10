// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_header_table.h"

#include "base/logging.h"
#include "net/spdy/hpack_constants.h"
#include "net/spdy/hpack_string_util.h"

namespace net {

HpackHeaderTable::HpackHeaderTable()
    : size_(0),
      max_size_(kDefaultHeaderTableSizeSetting) {}

HpackHeaderTable::~HpackHeaderTable() {}

uint32 HpackHeaderTable::GetEntryCount() const {
  size_t size = entries_.size();
  DCHECK_LE(size, kuint32max);
  return static_cast<uint32>(size);
}

const HpackEntry& HpackHeaderTable::GetEntry(uint32 index) const {
  CHECK_GE(index, 1u);
  CHECK_LE(index, GetEntryCount());
  return entries_[index-1];
}

HpackEntry* HpackHeaderTable::GetMutableEntry(uint32 index) {
  CHECK_GE(index, 1u);
  CHECK_LE(index, GetEntryCount());
  return &entries_[index-1];
}

void HpackHeaderTable::SetMaxSize(uint32 max_size) {
  max_size_ = max_size;
  while (size_ > max_size_) {
    CHECK(!entries_.empty());
    size_ -= entries_.back().Size();
    entries_.pop_back();
  }
}

void HpackHeaderTable::TryAddEntry(
    const HpackEntry& entry,
    uint32* index,
    std::vector<uint32>* removed_referenced_indices) {
  *index = 0;
  removed_referenced_indices->clear();

  // The algorithm used here is described in 3.3.3. We're assuming
  // that the given entry is caching the name/value.
  size_t target_size = 0;
  size_t size_t_max_size = static_cast<size_t>(max_size_);
  if (entry.Size() <= size_t_max_size) {
    // The conditional implies the difference can fit in 32 bits.
    target_size = size_t_max_size - entry.Size();
  }
  while (static_cast<size_t>(size_) > target_size) {
    DCHECK(!entries_.empty());
    if (entries_.back().IsReferenced()) {
      removed_referenced_indices->push_back(entries_.size());
    }
    size_ -= entries_.back().Size();
    entries_.pop_back();
  }

  if (entry.Size() <= size_t_max_size) {
    // Implied by the exit condition of the while loop above and the
    // condition of the if.
    DCHECK_LE(static_cast<size_t>(size_) + entry.Size(), size_t_max_size);
    size_ += entry.Size();
    *index = 1;
    entries_.push_front(entry);
  }
}

}  // namespace net
