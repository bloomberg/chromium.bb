// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_RANKED_LIST_H_
#define COMPONENTS_METRICS_LEAK_DETECTOR_RANKED_LIST_H_

#include <stddef.h>

#include <list>

#include "base/macros.h"
#include "components/metrics/leak_detector/custom_allocator.h"
#include "components/metrics/leak_detector/leak_detector_value_type.h"
#include "components/metrics/leak_detector/stl_allocator.h"

namespace metrics {
namespace leak_detector {

// RankedList lets you add entries consisting of a value-count pair, and
// automatically sorts them internally by count in descending order. This allows
// for the user of this list to put value-count pairs into this list without
// having to explicitly sort them by count.
class RankedList {
 public:
  using ValueType = LeakDetectorValueType;

  // A single entry in the RankedList. The RankedList sorts entries by |count|
  // in descending order.
  struct Entry {
    ValueType value;
    int count;

    // Create a < comparator for reverse sorting.
    bool operator<(const Entry& entry) const { return count > entry.count; }
  };

  // This class uses CustomAllocator to avoid recursive malloc hook invocation
  // when analyzing allocs and frees.
  using EntryList = std::list<Entry, STLAllocator<Entry, CustomAllocator>>;
  using const_iterator = EntryList::const_iterator;

  explicit RankedList(size_t max_size);
  ~RankedList();

  // For move semantics.
  RankedList(RankedList&& other);
  RankedList& operator=(RankedList&& other);

  // Accessors for begin() and end() const iterators.
  const_iterator begin() const { return entries_.begin(); }
  const_iterator end() const { return entries_.end(); }

  size_t size() const { return entries_.size(); }
  size_t max_size() const { return max_size_; }

  // Add a new value-count pair to the list. Does not check for existing entries
  // with the same value. Is an O(n) operation due to ordering.
  void Add(const ValueType& value, int count);

 private:
  // Max and min counts. Returns 0 if the list is empty.
  int max_count() const {
    return entries_.empty() ? 0 : entries_.begin()->count;
  }
  int min_count() const {
    return entries_.empty() ? 0 : entries_.rbegin()->count;
  }

  // Max number of items that can be stored in the list.
  size_t max_size_;

  // Points to the array of entries.
  std::list<Entry, STLAllocator<Entry, CustomAllocator>> entries_;

  DISALLOW_COPY_AND_ASSIGN(RankedList);
};

}  // namespace leak_detector
}  // namespace metrics

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_RANKED_LIST_H_
