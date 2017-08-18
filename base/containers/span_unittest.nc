// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/span.h"

#include <array>
#include <set>
#include <vector>

namespace base {

class Base {
};

class Derived : Base {
};

#if defined(NCTEST_DERIVED_TO_BASE_CONVERSION_DISALLOWED)  // [r"fatal error: no matching constructor for initialization of 'Span<base::Base \*>'"]

// Internally, this is represented as a pointer to pointers to Derived. An
// implicit conversion to a pointer to pointers to Base must not be allowed.
// If it were allowed, then something like this would be possible.
//   Cat** cats = GetCats();
//   Animals** animals = cats;
//   animals[0] = new Dog();  // Uhoh!
void WontCompile() {
  Span<Derived*> derived_span;
  Span<Base*> base_span(derived_span);
}

#elif defined(NCTEST_PTR_TO_CONSTPTR_CONVERSION_DISALLOWED)  // [r"fatal error: no matching constructor for initialization of 'Span<const int \*>'"]

// Similarly, converting a Span<int*> to Span<const int*> requires internally
// converting T** to const T**. This is also disallowed, as it would allow code
// to violate the contract of const.
void WontCompile() {
  Span<int*> non_const_span;
  Span<const int*> const_span(non_const_span);
}

#elif defined(NCTEST_STD_ARRAY_CONVERSION_DISALLOWED)  // [r"fatal error: no matching constructor for initialization of 'Span<int>'"]

// This isn't implemented today. Maybe it will be some day.
void WontCompile() {
  std::array<int, 3> array;
  Span<int> span(array);
}

#elif defined(NCTEST_CONST_CONTAINER_TO_MUTABLE_CONVERSION_DISALLOWED)  // [r"fatal error: no matching constructor for initialization of 'Span<int>'"]

// A const container should not be convertible to a mutable span.
void WontCompile() {
  const std::vector<int> v = {1, 2, 3};
  Span<int> span(v);
}

#elif defined(NCTEST_STD_SET_CONVERSION_DISALLOWED)  // [r"fatal error: no matching constructor for initialization of 'Span<int>'"]

// A std::set() should not satisfy the requirements for conversion to a Span.
void WontCompile() {
  std::set<int> set;
  Span<int> span(set);
}

#elif defined(NCTEST_MAKE_SPAN_FROM_SET_CONVERSION_DISALLOWED)  // [r"fatal error: no matching function for call to 'MakeSpan'"]

// A std::set() should not satisfy the requirements for conversion to a Span.
void WontCompile() {
  std::set<int> set;
  auto span = MakeSpan(set);
}

#endif

}  // namespace base
