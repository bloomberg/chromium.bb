// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/unittest_test_suite.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/test/test_suite.h"
#if !defined(OS_IOS)
#include "content/test/test_webkit_platform_support.h"
#endif
#include "third_party/WebKit/public/web/WebKit.h"

namespace content {

UnitTestTestSuite::UnitTestTestSuite(base::TestSuite* test_suite)
    : test_suite_(test_suite) {
  DCHECK(test_suite);
#if !defined(OS_IOS)
  platform_support_.reset(new TestWebKitPlatformSupport);
#endif
}

UnitTestTestSuite::~UnitTestTestSuite() {
#if !defined(OS_IOS)
  platform_support_.reset();
#endif
}

int UnitTestTestSuite::Run() {
  return test_suite_->Run();
}

}  // namespace content
