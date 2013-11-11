// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_TESTS_TEST_SUPPORT_H_
#define MOJO_PUBLIC_TESTS_TEST_SUPPORT_H_

#include "base/callback.h"

namespace mojo {
namespace test {

// Run |single_iteration| an appropriate number of times and report its
// performance appropriately. (This actually runs |single_iteration| for a fixed
// amount of time and reports the number of iterations per unit time.)
void IterateAndReportPerf(const char* test_name,
                          base::Callback<void()> single_iteration);

}  // namespace test
}  // namespace mojo

#endif  // MOJO_PUBLIC_TESTS_TEST_SUPPORT_H_
