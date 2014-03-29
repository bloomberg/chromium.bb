// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the C Mojo system macros and consists of "positive" tests,
// i.e., those verifying that things work (without compile errors, or even
// warnings if warnings are treated as errors).
// TODO(vtl): Fix no-compile tests (which are all disabled; crbug.com/105388)
// and write some "negative" tests.

#include "mojo/public/c/system/macros.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

TEST(MacrosTest, AllowUnused) {
  // Test that no warning/error is issued even though |x| is unused.
  int x MOJO_ALLOW_UNUSED = 123;
}

int MustUseReturnedResult() MOJO_WARN_UNUSED_RESULT;
int MustUseReturnedResult() {
  return 456;
}

TEST(MacrosTest, WarnUnusedResult) {
  if (!MustUseReturnedResult())
    abort();
}

// First test |MOJO_COMPILE_ASSERT()| in a global scope.
MOJO_COMPILE_ASSERT(sizeof(int64_t) == 2 * sizeof(int32_t),
                    bad_compile_assert_failure_in_global_scope);

TEST(MacrosTest, CompileAssert) {
  // Then in a local scope.
  MOJO_COMPILE_ASSERT(sizeof(int32_t) == 2 * sizeof(int16_t),
                      bad_compile_assert_failure);
}

}  // namespace
}  // namespace mojo
