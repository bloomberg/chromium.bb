// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_UNIQUE_PTR_COMPARATOR_H_
#define BASE_CONTAINERS_UNIQUE_PTR_COMPARATOR_H_

#include <memory>

namespace base {

// This transparent comparator allows to lookup by raw pointer in
// a container of unique pointers. This functionality is based on C++14
// extensions to std::set/std::map interface, and can also be used
// with base::flat_set/base::flat_map.
//
// Example usage:
//   Foo* foo = ...
//   std::set<std::unique_ptr<Foo>, base::UniquePtrComparator> set;
//   set.insert(std::unique_ptr<Foo>(foo));
//   ...
//   auto it = set.find(foo);
//   EXPECT_EQ(foo, it->get());
//
// You can find more information about transparent comparisons here:
// http://en.cppreference.com/w/cpp/utility/functional/less_void
struct UniquePtrComparator {
  using is_transparent = int;

  template <typename T>
  bool operator()(const std::unique_ptr<T>& lhs,
                  const std::unique_ptr<T>& rhs) const {
    return lhs < rhs;
  }

  template <typename T>
  bool operator()(const T* lhs, const std::unique_ptr<T>& rhs) const {
    return lhs < rhs.get();
  }

  template <typename T>
  bool operator()(const std::unique_ptr<T>& lhs, const T* rhs) const {
    return lhs.get() < rhs;
  }
};

}  // namespace base

#endif  // BASE_CONTAINERS_UNIQUE_PTR_COMPARATOR_H_
