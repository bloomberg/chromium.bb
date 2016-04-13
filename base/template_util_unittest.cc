// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/template_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

// is_non_const_reference<Type>
static_assert(!is_non_const_reference<int>::value, "IsNonConstReference");
static_assert(!is_non_const_reference<const int&>::value,
              "IsNonConstReference");
static_assert(is_non_const_reference<int&>::value, "IsNonConstReference");

class AssignParent {};
class AssignChild : AssignParent {};

// is_assignable<Type1, Type2>
static_assert(!is_assignable<int, int>::value, "IsAssignable");  // 1 = 1;
static_assert(!is_assignable<int, double>::value, "IsAssignable");
static_assert(is_assignable<int&, int>::value, "IsAssignable");
static_assert(is_assignable<int&, double>::value, "IsAssignable");
static_assert(is_assignable<int&, int&>::value, "IsAssignable");
static_assert(is_assignable<int&, int const&>::value, "IsAssignable");
static_assert(!is_assignable<int const&, int>::value, "IsAssignable");
static_assert(!is_assignable<AssignParent&, AssignChild>::value,
              "IsAssignable");
static_assert(!is_assignable<AssignChild&, AssignParent>::value,
              "IsAssignable");

struct AssignCopy {};
struct AssignNoCopy {
  AssignNoCopy& operator=(AssignNoCopy&&) { return *this; }
  AssignNoCopy& operator=(const AssignNoCopy&) = delete;
};
struct AssignNoMove {
  AssignNoMove& operator=(AssignNoMove&&) = delete;
  AssignNoMove& operator=(const AssignNoMove&) = delete;
};

static_assert(is_copy_assignable<AssignCopy>::value, "IsCopyAssignable");
static_assert(!is_copy_assignable<AssignNoCopy>::value, "IsCopyAssignable");

static_assert(is_move_assignable<AssignCopy>::value, "IsMoveAssignable");
static_assert(is_move_assignable<AssignNoCopy>::value, "IsMoveAssignable");
static_assert(!is_move_assignable<AssignNoMove>::value, "IsMoveAssignable");

}  // namespace
}  // namespace base
