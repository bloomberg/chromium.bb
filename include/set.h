// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_SET_H__
#define GESTURES_SET_H__

#include <algorithm>

// This is a set class that doesn't call out to malloc/free. Many of the
// names were chosen to mirror std::set.

// A template parameter to this class is kMaxSize, which is the max number
// of elements that such a set can hold. Internally, it contains an array
// of Elt objects.

// Differences from std::set:
// - erase() returns void here (would be easy to make match std::set)
// - Many methods are unimplemented
// - insert()/erase() invalidate existing iterators
// - Currently, the Elt type should be a POD type or aggregate of PODs,
//   since ctors/dtors aren't called propertly on Elt objects.

namespace gestures {

template<typename Elt, int kMaxSize>
class set {
 public:
  typedef Elt* iterator;
  typedef const Elt* const_iterator;

  set() : size_(0) {}
  set(const set<Elt, kMaxSize>& that) {
    size_ = that.size_;
    std::copy(that.begin(), that.end(), begin());
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
  void insert(const Elt& value) {  // TODO(adlr): return pair<iterator, bool>
    if (size_ == kMaxSize)
      return;  // TODO(adlr): log error
    if (find(value) != end())
      return;  // already in set
    buffer_[size_] = value;
    ++size_;
  }

  // Returns number of elements removed (0 or 1).
  // Unlike std::set, invalidates iterators.
  size_t erase(const Elt& value) {
    iterator ptr = find(value);
    if (ptr == end())
      return 0;
    std::copy(ptr + 1, end(), ptr);
    --size_;
    return 1;
  }
  void clear() { size_ = 0; }

  template<int kThatSize>
  set<Elt, kMaxSize>& operator=(const set<Elt, kThatSize>& that) {
    if (that.size() > kMaxSize)
      // Uh oh, that won't fit into this
      return *this;  // TODO(adlr): Log error
    std::copy(that.begin(), that.end(), begin());
    size_ = that.size();
    return *this;
  }

 private:
  Elt buffer_[kMaxSize];
  unsigned short size_;
};

template<typename Elt, int kLeftMaxSize, int kRightMaxSize>
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
template<typename Elt, int kLeftMaxSize, int kRightMaxSize>
inline bool operator!=(const set<Elt, kLeftMaxSize>& left,
                       const set<Elt, kRightMaxSize>& right) {
  return !(left == right);
}

}  // namespace gestures

#endif  // GESTURES_SET_H__
