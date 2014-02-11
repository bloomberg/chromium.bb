// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains things to be provided by the environment hosting tests.
// TODO(vtl): Add a suitable "standalone" implementation. (This will make sense
// when we can build apps/tests in, e.g., NaCl.)

#ifndef MOJO_PUBLIC_ENVIRONMENT_TEST_SUPPORT_H_
#define MOJO_PUBLIC_ENVIRONMENT_TEST_SUPPORT_H_

namespace mojo {
namespace test {

// Like Chromium's |base::LogPerfResult()| (base/perf_log.h; note that the
// environment-specific test runner is expected to initialize/finalize the perf
// log as necessary):
void LogPerfResult(const char* test_name, double value, const char* units);

}  // namespace test
}  // namespace mojo

#endif  // MOJO_PUBLIC_ENVIRONMENT_TEST_SUPPORT_H_
