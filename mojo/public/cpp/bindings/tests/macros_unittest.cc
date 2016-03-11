// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the C++ Mojo system macros and consists of "positive" tests,
// i.e., those verifying that things work (without compile errors, or even
// warnings if warnings are treated as errors).
// TODO(vtl): Maybe rename "MacrosCppTest" -> "MacrosTest" if/when this gets
// compiled into a different binary from the C API tests.
// TODO(vtl): Fix no-compile tests (which are all disabled; crbug.com/105388)
// and write some "negative" tests.

#include "mojo/public/cpp/bindings/lib/macros.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <utility>

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

// The test for |MOJO_STATIC_CONST_MEMBER_DEFINITION| is really a compile/link
// test. To test it fully would really require a header file and multiple .cc
// files, but we'll just cursorily verify it.
//
// This is defined outside of an anonymous namespace because
// MOJO_STATIC_CONST_MEMBER_DEFINITION may not be used on internal symbols.
struct StructWithStaticConstMember {
  static const int kStaticConstMember = 123;
};
MOJO_STATIC_CONST_MEMBER_DEFINITION
const int StructWithStaticConstMember::kStaticConstMember;

namespace {

// Note: MSVS is very strict (and arguably buggy) about warnings for classes
// defined in a local scope, so define these globally.
struct TestOverrideBaseClass {
  virtual ~TestOverrideBaseClass() {}
  virtual void ToBeOverridden() {}
  virtual void AlsoToBeOverridden() = 0;
};

struct TestOverrideSubclass : public TestOverrideBaseClass {
  ~TestOverrideSubclass() override {}
  void ToBeOverridden() override {}
  void AlsoToBeOverridden() override {}
};

TEST(MacrosCppTest, Override) {
  TestOverrideSubclass x;
  x.ToBeOverridden();
  x.AlsoToBeOverridden();
}

// Use it, to make sure things get linked in and to avoid any warnings about
// unused things.
TEST(MacrosCppTest, StaticConstMemberDefinition) {
  EXPECT_EQ(123, StructWithStaticConstMember::kStaticConstMember);
}

}  // namespace
}  // namespace mojo
