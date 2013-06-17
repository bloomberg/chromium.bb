// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/unittest_test_suite.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/test/test_suite.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/web/WebKit.h"

namespace content {

#if !defined(OS_IOS)
// A stubbed out WebKit platform support impl.
class UnitTestTestSuite::UnitTestWebKitPlatformSupport
    : public WebKit::Platform {
 public:
  UnitTestWebKitPlatformSupport() {}
  virtual ~UnitTestWebKitPlatformSupport() {}
  virtual void cryptographicallyRandomValues(unsigned char* buffer,
                                             size_t length) OVERRIDE {
    base::RandBytes(buffer, length);
  }
  virtual const unsigned char* getTraceCategoryEnabledFlag(
      const char* categoryName) {
    // Causes tracing macros to be disabled.
    static const unsigned char kEnabled = 0;
    return &kEnabled;
  }
};
#endif  // !OS_IOS

UnitTestTestSuite::UnitTestTestSuite(base::TestSuite* test_suite)
    : test_suite_(test_suite) {
  DCHECK(test_suite);
#if !defined(OS_IOS)
  webkit_platform_support_.reset(new UnitTestWebKitPlatformSupport);
  WebKit::initialize(webkit_platform_support_.get());
#endif
}

UnitTestTestSuite::~UnitTestTestSuite() {
#if !defined(OS_IOS)
  WebKit::shutdown();
#endif
}

int UnitTestTestSuite::Run() {
  return test_suite_->Run();
}

}  // namespace content
