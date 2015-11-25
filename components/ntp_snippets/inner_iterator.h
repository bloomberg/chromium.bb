// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_INNER_ITERATOR_H_
#define COMPONENTS_NTP_SNIPPETS_INNER_ITERATOR_H_

#include <iterator>

namespace ntp_snippets {

// Build an iterator returning a dereference of an original iterator. For
// example  an object with an instance variable of type
// std::vector<std::unique_ptr<SomeClass>> instance_;
// could serve a const_iterator (of const SomeClass&) by simply doing the
// following:
//
// using const_iterator = InnerIterator<
//     std::vector<std::unique_ptr<SomeClass>>::const_iterator,
//     const Someclass>;
// [...]
// const_iterator begin() { return const_iterator(instance_.begin()); }
//
template <typename I,  // type of original iterator
          typename R   // type returned by this iterator
          >
class InnerIterator {
 public:
  using difference_type = typename std::iterator_traits<I>::difference_type;
  using value_type = R;
  using pointer = R*;
  using reference = R&;
  using iterator_category = std::random_access_iterator_tag;

  // Construction from an iterator.
  explicit InnerIterator(I from) : it_(from) {}

  // Operators *, ->, and [] are first forwarded to the contained
  // iterator, then extract the inner member.
  reference operator*() const { return **it_; }
  pointer operator->() const { return &(**it_); }
  reference operator[](difference_type n) const { return *(it_[n]); }

  // All operators that have to do with position are forwarded
  // to the contained iterator.
  InnerIterator& operator++() {
    ++it_;
    return *this;
  }
  InnerIterator operator++(int) {
    I tmp(it_);
    it_++;
    return InnerIterator(tmp);
  }
  InnerIterator& operator--() {
    --it_;
    return *this;
  }
  InnerIterator operator--(int) {
    I tmp(it_);
    it_--;
    return InnerIterator(tmp);
  }
  InnerIterator& operator+=(difference_type n) {
    it_ += n;
    return *this;
  }
  InnerIterator operator+(difference_type n) const {
    I tmp(it_);
    tmp += n;
    return InnerIterator(tmp);
  }
  InnerIterator& operator-=(difference_type n) {
    it_ -= n;
    return *this;
  }
  InnerIterator operator-(difference_type n) const {
    I tmp(it_);
    tmp -= n;
    return InnerIterator(tmp);
  }

  bool operator==(const InnerIterator<I, R>& rhs) const {
    return it_ == rhs.it_;
  }
  bool operator!=(const InnerIterator<I, R>& rhs) const {
    return it_ != rhs.it_;
  }

 protected:
  I it_;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_INNER_ITERATOR_H_
