// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_TESTS_TEST_SUPPORT_H_
#define MOJO_PUBLIC_TESTS_TEST_SUPPORT_H_

// Note: This header should be compilable as C.

#include "mojo/public/tests/test_support_export.h"

#ifdef __cplusplus
extern "C" {
#endif

MOJO_TEST_SUPPORT_EXPORT void MojoTestSupportLogPerfResult(
    const char* test_name,
    double value,
    const char* units);

#ifdef __cplusplus
}  // extern "C"
#endif

#ifdef __cplusplus
namespace mojo {
namespace test {

inline void LogPerfResult(const char* test_name,
                          double value,
                          const char* units) {
  MojoTestSupportLogPerfResult(test_name, value, units);
}

}  // namespace test
}  // namespace mojo
#endif  // __cplusplus

#endif  // MOJO_PUBLIC_TESTS_TEST_SUPPORT_H_
