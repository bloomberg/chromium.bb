// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_CHECKED_ITERATORS_H_
#define BASE_CONTAINERS_CHECKED_ITERATORS_H_

#include <iterator>
#include <memory>
#include <type_traits>

#include "base/containers/util.h"
#include "base/logging.h"

namespace base {

template <typename T>
class CheckedRandomAccessIterator {
 public:
  using difference_type = std::ptrdiff_t;
  using value_type = std::remove_cv_t<T>;
  using pointer = T*;
  using reference = T&;
  using iterator_category = std::random_access_iterator_tag;

  // Required for converting constructor below.
  template <typename U>
  friend class CheckedRandomAccessIterator;

  CheckedRandomAccessIterator() = default;
  CheckedRandomAccessIterator(T* start, const T* end)
      : CheckedRandomAccessIterator(start, start, end) {}
  CheckedRandomAccessIterator(const T* start, T* current, const T* end)
      : start_(start), current_(current), end_(end) {
    CHECK(start <= current);
    CHECK(current <= end);
  }
  CheckedRandomAccessIterator(const CheckedRandomAccessIterator& other) =
      default;

  // Converting constructor allowing conversions like CRAI<T> to CRAI<const T>,
  // but disallowing CRAI<const T> to CRAI<T> or CRAI<Derived> to CRAI<Base>,
  // which are unsafe. Furthermore, this is the same condition as used by the
  // converting constructors of std::span<T> and std::unique_ptr<T[]>.
  // See https://wg21.link/n4042 for details.
  template <
      typename U,
      std::enable_if_t<std::is_convertible<U (*)[], T (*)[]>::value>* = nullptr>
  CheckedRandomAccessIterator(const CheckedRandomAccessIterator<U>& other)
      : start_(other.start_), current_(other.current_), end_(other.end_) {
    // We explicitly don't delegate to the 3-argument constructor here. Its
    // CHECKs would be redundant, since we expect |other| to maintain its own
    // invariant. However, DCHECKs never hurt anybody. Presumably.
    DCHECK(other.start_ <= other.current_);
    DCHECK(other.current_ <= other.end_);
  }

  ~CheckedRandomAccessIterator() = default;

  CheckedRandomAccessIterator& operator=(
      const CheckedRandomAccessIterator& other) = default;

  bool operator==(const CheckedRandomAccessIterator& other) const {
    CheckComparable(other);
    return current_ == other.current_;
  }

  bool operator!=(const CheckedRandomAccessIterator& other) const {
    CheckComparable(other);
    return current_ != other.current_;
  }

  bool operator<(const CheckedRandomAccessIterator& other) const {
    CheckComparable(other);
    return current_ < other.current_;
  }

  bool operator<=(const CheckedRandomAccessIterator& other) const {
    CheckComparable(other);
    return current_ <= other.current_;
  }

  bool operator>(const CheckedRandomAccessIterator& other) const {
    CheckComparable(other);
    return current_ > other.current_;
  }

  bool operator>=(const CheckedRandomAccessIterator& other) const {
    CheckComparable(other);
    return current_ >= other.current_;
  }

  CheckedRandomAccessIterator& operator++() {
    CHECK(current_ != end_);
    ++current_;
    return *this;
  }

  CheckedRandomAccessIterator operator++(int) {
    CheckedRandomAccessIterator old = *this;
    ++*this;
    return old;
  }

  CheckedRandomAccessIterator& operator--() {
    CHECK(current_ != start_);
    --current_;
    return *this;
  }

  CheckedRandomAccessIterator& operator--(int) {
    CheckedRandomAccessIterator old = *this;
    --*this;
    return old;
  }

  CheckedRandomAccessIterator& operator+=(difference_type rhs) {
    if (rhs > 0) {
      CHECK_LE(rhs, end_ - current_);
    } else {
      CHECK_LE(-rhs, current_ - start_);
    }
    current_ += rhs;
    return *this;
  }

  CheckedRandomAccessIterator operator+(difference_type rhs) const {
    CheckedRandomAccessIterator it = *this;
    it += rhs;
    return it;
  }

  CheckedRandomAccessIterator& operator-=(difference_type rhs) {
    if (rhs < 0) {
      CHECK_LE(rhs, end_ - current_);
    } else {
      CHECK_LE(-rhs, current_ - start_);
    }
    current_ -= rhs;
    return *this;
  }

  CheckedRandomAccessIterator operator-(difference_type rhs) const {
    CheckedRandomAccessIterator it = *this;
    it -= rhs;
    return it;
  }

  friend difference_type operator-(const CheckedRandomAccessIterator& lhs,
                                   const CheckedRandomAccessIterator& rhs) {
    CHECK(lhs.start_ == rhs.start_);
    CHECK(lhs.end_ == rhs.end_);
    return lhs.current_ - rhs.current_;
  }

  reference operator*() const {
    CHECK(current_ != end_);
    return *current_;
  }

  pointer operator->() const {
    CHECK(current_ != end_);
    return current_;
  }

  reference operator[](difference_type rhs) const {
    CHECK_GE(rhs, 0);
    CHECK_LT(rhs, end_ - current_);
    return current_[rhs];
  }

  static bool IsRangeMoveSafe(const CheckedRandomAccessIterator& from_begin,
                              const CheckedRandomAccessIterator& from_end,
                              const CheckedRandomAccessIterator& to)
      WARN_UNUSED_RESULT {
    if (from_end < from_begin)
      return false;
    const auto from_begin_uintptr = get_uintptr(from_begin.current_);
    const auto from_end_uintptr = get_uintptr(from_end.current_);
    const auto to_begin_uintptr = get_uintptr(to.current_);
    const auto to_end_uintptr =
        get_uintptr((to + std::distance(from_begin, from_end)).current_);

    return to_begin_uintptr >= from_end_uintptr ||
           to_end_uintptr <= from_begin_uintptr;
  }

 private:
  void CheckComparable(const CheckedRandomAccessIterator& other) const {
    CHECK_EQ(start_, other.start_);
    CHECK_EQ(end_, other.end_);
  }

  const T* start_ = nullptr;
  T* current_ = nullptr;
  const T* end_ = nullptr;
};

template <typename T>
using CheckedRandomAccessConstIterator = CheckedRandomAccessIterator<const T>;

}  // namespace base

#endif  // BASE_CONTAINERS_CHECKED_ITERATORS_H_
