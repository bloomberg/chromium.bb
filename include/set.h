// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_SET_H__
#define GESTURES_SET_H__

#include <algorithm>

#include "gestures/include/gestures.h"
#include "gestures/include/logging.h"

// This is a set class that doesn't call out to malloc/free. Many of the
// names were chosen to mirror std::set.

// A template parameter to this class is kMaxSize, which is the max number
// of elements that such a set can hold. Internally, it contains an array
// of Elt objects.

// Differences from std::set:
// - Many methods are unimplemented
// - insert()/erase() invalidate existing iterators
// - Currently, the Elt type should be a POD type or aggregate of PODs,
//   since ctors/dtors aren't called propertly on Elt objects.

namespace gestures {

template<typename Elt, size_t kMaxSize>
class set {
 public:
  typedef Elt* iterator;
  typedef const Elt* const_iterator;

  set() : size_(0) {}
  set(const set<Elt, kMaxSize>& that) {
    *this = that;
  }
  template<size_t kThatSize>
  set(const set<Elt, kThatSize>& that) {
    *this = that;
  }

  const_iterator begin() const { return buffer_; }
  const_iterator end() const {
    const_iterator ret = &buffer_[size_];
    return ret;
  }
  const_iterator find(const Elt& value) const {
    for (size_t i = 0; i < size_; ++i)
      if (buffer_[i] == value)
        return &buffer_[i];
    return end();
  }
  size_t size() const { return size_; }
  bool empty() const { return size() == 0; }
  // Non-const versions:
  iterator begin() {
    return const_cast<iterator>(
        const_cast<const set<Elt, kMaxSize>*>(this)->begin());
  }
  iterator end() {
    return const_cast<iterator>(
        const_cast<const set<Elt, kMaxSize>*>(this)->end());
  }
  iterator find(const Elt& value) {
    return const_cast<iterator>(
        const_cast<const set<Elt, kMaxSize>*>(this)->find(value));
  }

  // Unlike std::set, invalidates iterators.
  std::pair<iterator, bool> insert(const Elt& value) {
    iterator it = find(value);
    if (it != end())
      return std::make_pair(it, false);
    if (size_ == kMaxSize) {
      Err("set::insert: out of space!");
      return std::make_pair(end(), false);
    }
    iterator new_elt = &buffer_[size_];
    new (new_elt) Elt(value);
    ++size_;
    return std::make_pair(new_elt, true);
  }

  // Returns number of elements removed (0 or 1).
  // Unlike std::set, invalidates iterators.
  size_t erase(const Elt& value) {
    iterator ptr = find(value);
    if (ptr == end())
      return 0;
    erase(ptr);
    return 1;
  }
  void erase(iterator it) {
    std::copy(it + 1, end(), it);
    (*(end() - 1)).~Elt();
    --size_;
  }
  void clear() {
    for (iterator it = begin(), e = end(); it != e; ++it)
      (*it).~Elt();
    size_ = 0;
  }

  template<size_t kThatSize>
  set<Elt, kMaxSize>& operator=(const set<Elt, kThatSize>& that) {
    if (that.size() > kMaxSize) {
      // Uh oh, that won't fit into this
      Err("set::operator=: out of space!");
      return *this;
    }
    std::copy(that.begin(), that.end(), begin());
    if (size_ > that.size())
      for (iterator it = begin() + that.size(), e = end(); it != e; ++it)
        (*it).~Elt();
    size_ = that.size();
    return *this;
  }

 private:
  Elt buffer_[kMaxSize];
  unsigned short size_;
};

template<typename Elt, size_t kLeftMaxSize, size_t kRightMaxSize>
inline bool operator==(const set<Elt, kLeftMaxSize>& left,
                       const set<Elt, kRightMaxSize>& right) {
  if (left.size() != right.size())
    return false;
  for (typename set<Elt, kLeftMaxSize>::const_iterator left_it = left.begin(),
           left_end = left.end(); left_it != left_end; ++left_it)
    if (right.find(*left_it) == right.end())
      return false;
  return true;
}
template<typename Elt, size_t kLeftMaxSize, size_t kRightMaxSize>
inline bool operator!=(const set<Elt, kLeftMaxSize>& left,
                       const set<Elt, kRightMaxSize>& right) {
  return !(left == right);
}

template<typename Set, typename Elt>
inline bool SetContainsValue(const Set& the_set,
                             const Elt& elt) {
  return the_set.find(elt) != the_set.end();
}

// Removes all elements from |reduced| which are not in |required|
template<typename ReducedSet, typename RequiredSet>
inline void SetRemoveMissing(ReducedSet* reduced, const RequiredSet& required) {
  typename ReducedSet::iterator it = reduced->begin();
  while (it != reduced->end()) {
    if (SetContainsValue(required, *it)) {
      ++it;
      continue;
    }
    if (it == reduced->begin()) {
      reduced->erase(it);
      it = reduced->begin();
    } else {
      typename ReducedSet::iterator it_copy = it;
      --it;
      reduced->erase(it_copy);
      ++it;
    }
  }
}

// Removes any ids from the set that are not finger ids in hs.
template<size_t kSetSize>
void RemoveMissingIdsFromSet(set<short, kSetSize>* the_set,
                             const HardwareState& hs) {
  short old_ids[the_set->size()];
  size_t old_ids_len = 0;
  for (typename set<short, kSetSize>::const_iterator it = the_set->begin();
       it != the_set->end(); ++it)
    if (!hs.GetFingerState(*it))
      old_ids[old_ids_len++] = *it;
  for (size_t i = 0; i < old_ids_len; i++)
    the_set->erase(old_ids[i]);
}

}  // namespace gestures

#endif  // GESTURES_SET_H__
