// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Simple max sized map. Automatically deletes the oldest element when the
// max limit is reached.
// Note: the ConstIterator will NOT be valid after an Insert or RemoveAll.
#ifndef NET_QUIC_CONGESTION_CONTROL_QUIC_MAX_SIZED_MAP_H_
#define NET_QUIC_CONGESTION_CONTROL_QUIC_MAX_SIZED_MAP_H_

#include <stdlib.h>

#include <list>
#include <map>

#include "base/basictypes.h"

namespace net {

template <class Key, class Value>
class QuicMaxSizedMap {
 public:
  typedef typename std::multimap<Key, Value>::const_iterator ConstIterator;

  explicit QuicMaxSizedMap(size_t max_numer_of_items)
      : max_numer_of_items_(max_numer_of_items) {
  }

  size_t MaxSize() const {
    return max_numer_of_items_;
  }

  size_t Size() const {
    return table_.size();
  }

  void Insert(const Key& k, const Value& value) {
    if (Size() == MaxSize()) {
      ListIterator list_it = insert_order_.begin();
      table_.erase(*list_it);
      insert_order_.pop_front();
    }
    TableIterator it = table_.insert(std::pair<Key, Value>(k, value));
    insert_order_.push_back(it);
  }

  void RemoveAll() {
    table_.clear();
    insert_order_.clear();
  }

  // STL style const_iterator support.
  ConstIterator Find(const Key& k) const {
    return table_.find(k);
  }

  ConstIterator Begin() const {
    return ConstIterator(table_.begin());
  }

  ConstIterator End() const {
    return ConstIterator(table_.end());
  }

 private:
  typedef typename std::multimap<Key, Value>::iterator TableIterator;
  typedef typename std::list<TableIterator>::iterator ListIterator;

  const size_t max_numer_of_items_;
  std::multimap<Key, Value> table_;
  std::list<TableIterator> insert_order_;

  DISALLOW_COPY_AND_ASSIGN(QuicMaxSizedMap);
};

}  // namespace net
#endif  // NET_QUIC_CONGESTION_CONTROL_QUIC_MAX_SIZED_MAP_H_
