// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "mojo/public/interfaces/bindings/tests/test_structs.mojom.h"

namespace mojo {
namespace test {

#if defined(NCTEST_ZERO_IMPLICITLY_CAST_TO_STRUCT_PTR)  // [r"fatal error: conversion function from 'int' to 'mojo::test::RectPairPtr' .* invokes a deleted function"]

// Imagine a refactoring where a parameter type changes from |int| to
// |RectPairPtr| - we want to fail compiling all the callsites that keep
// passing an integer.  In particular, 0 should not implicitly cast to
// RectPairPtr.
void Foo(RectPairPtr) {}
void WontCompile() {
  Foo(0);  // This should fail to compile.
}

#elif defined(NCTEST_ZERO_ASSIGNED_TO_STRUCT_PTR)  // [r"fatal error: overload resolution selected deleted operator '='"]

// See description of NCTEST_ZERO_IMPLICITLY_CAST_TO_STRUCT_PTR for the
// rationale behind this test.
void WontCompile() {
  RectPairPtr bar;
  bar = 0;  // This should fail to compile.
}

#elif defined(NCTEST_ZERO_IMPLICITLY_CAST_TO_INLINED_STRUCT_PTR)  // [r"fatal error: conversion function from 'int' to 'mojo::test::SingleBoolStructPtr' .* invokes a deleted function"]

// See description of NCTEST_ZERO_IMPLICITLY_CAST_TO_STRUCT_PTR for the
// rationale behind this test.
void Foo(SingleBoolStructPtr) {}
void WontCompile() {
  Foo(0);  // This should fail to compile.
}

#elif defined(NCTEST_ZERO_ASSIGNED_TO_INLINED_STRUCT_PTR)  // [r"fatal error: overload resolution selected deleted operator '='"]

// See description of NCTEST_ZERO_IMPLICITLY_CAST_TO_STRUCT_PTR for the
// rationale behind this test.
void WontCompile() {
  SingleBoolStructPtr bar;
  bar = 0;  // This should fail to compile.
}

#endif

}  // namespace test
}  // namespace mojo
