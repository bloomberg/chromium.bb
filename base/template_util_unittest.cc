// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/template_util.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

struct AStruct {};
class AClass {};
enum AnEnum {};

class Parent {};
class Child : public Parent {};

// is_pointer<Type>
static_assert(!is_pointer<int>::value, "IsPointer");
static_assert(!is_pointer<int&>::value, "IsPointer");
static_assert(is_pointer<int*>::value, "IsPointer");
static_assert(is_pointer<const int*>::value, "IsPointer");

// is_array<Type>
static_assert(!is_array<int>::value, "IsArray");
static_assert(!is_array<int*>::value, "IsArray");
static_assert(!is_array<int (*)[3]>::value, "IsArray");
static_assert(is_array<int[]>::value, "IsArray");
static_assert(is_array<const int[]>::value, "IsArray");
static_assert(is_array<int[3]>::value, "IsArray");

// is_non_const_reference<Type>
static_assert(!is_non_const_reference<int>::value, "IsNonConstReference");
static_assert(!is_non_const_reference<const int&>::value,
              "IsNonConstReference");
static_assert(is_non_const_reference<int&>::value, "IsNonConstReference");

// is_convertible<From, To>

// Extra parens needed to make preprocessor macro parsing happy. Otherwise,
// it sees the equivalent of:
//
//     (is_convertible < Child), (Parent > ::value)
//
// Silly C++.
static_assert((is_convertible<Child, Parent>::value), "IsConvertible");
static_assert(!(is_convertible<Parent, Child>::value), "IsConvertible");
static_assert(!(is_convertible<Parent, AStruct>::value), "IsConvertible");
static_assert((is_convertible<int, double>::value), "IsConvertible");
static_assert((is_convertible<int*, void*>::value), "IsConvertible");
static_assert(!(is_convertible<void*, int*>::value), "IsConvertible");

// Array types are an easy corner case.  Make sure to test that
// it does indeed compile.
static_assert(!(is_convertible<int[10], double>::value), "IsConvertible");
static_assert(!(is_convertible<double, int[10]>::value), "IsConvertible");
static_assert((is_convertible<int[10], int*>::value), "IsConvertible");

// is_same<Type1, Type2>
static_assert(!(is_same<Child, Parent>::value), "IsSame");
static_assert(!(is_same<Parent, Child>::value), "IsSame");
static_assert((is_same<Parent, Parent>::value), "IsSame");
static_assert((is_same<int*, int*>::value), "IsSame");
static_assert((is_same<int, int>::value), "IsSame");
static_assert((is_same<void, void>::value), "IsSame");
static_assert(!(is_same<int, double>::value), "IsSame");

// is_class<Type>
static_assert(is_class<AStruct>::value, "IsClass");
static_assert(is_class<AClass>::value, "IsClass");
static_assert(!is_class<AnEnum>::value, "IsClass");
static_assert(!is_class<int>::value, "IsClass");
static_assert(!is_class<char*>::value, "IsClass");
static_assert(!is_class<int&>::value, "IsClass");
static_assert(!is_class<char[3]>::value, "IsClass");

static_assert(!is_member_function_pointer<int>::value,
              "IsMemberFunctionPointer");
static_assert(!is_member_function_pointer<int*>::value,
              "IsMemberFunctionPointer");
static_assert(!is_member_function_pointer<void*>::value,
              "IsMemberFunctionPointer");
static_assert(!is_member_function_pointer<AStruct>::value,
              "IsMemberFunctionPointer");
static_assert(!is_member_function_pointer<AStruct*>::value,
              "IsMemberFunctionPointer");
static_assert(!is_member_function_pointer<void (*)()>::value,
              "IsMemberFunctionPointer");
static_assert(!is_member_function_pointer<int (*)(int)>::value,
              "IsMemberFunctionPointer");
static_assert(!is_member_function_pointer<int (*)(int, int)>::value,
              "IsMemberFunctionPointer");

static_assert(is_member_function_pointer<void (AStruct::*)()>::value,
              "IsMemberFunctionPointer");
static_assert(is_member_function_pointer<void (AStruct::*)(int)>::value,
              "IsMemberFunctionPointer");
static_assert(is_member_function_pointer<int (AStruct::*)(int)>::value,
              "IsMemberFunctionPointer");
static_assert(is_member_function_pointer<int (AStruct::*)(int) const>::value,
              "IsMemberFunctionPointer");
static_assert(is_member_function_pointer<int (AStruct::*)(int, int)>::value,
              "IsMemberFunctionPointer");

}  // namespace
}  // namespace base
