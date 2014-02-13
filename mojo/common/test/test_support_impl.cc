// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/test/test_support_impl.h"

#include "base/test/perf_log.h"

namespace mojo {
namespace test {

TestSupportImpl::TestSupportImpl() {
}

TestSupportImpl::~TestSupportImpl() {
}

void TestSupportImpl::LogPerfResult(const char* test_name,
                                    double value,
                                    const char* units) {
  base::LogPerfResult(test_name, value, units);
}

}  // namespace test
}  // namespace mojo
