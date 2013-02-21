// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/unittest_test_suite.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/test/test_suite.h"
#include "third_party/WebKit/Source/Platform/chromium/public/Platform.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "webkit/compositor_bindings/web_compositor_support_impl.h"

namespace content {

#if !defined(OS_IOS)
// A stubbed out WebKit platform support impl.
class UnitTestTestSuite::UnitTestWebKitPlatformSupport
    : public WebKit::WebKitPlatformSupport {
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

  virtual WebKit::WebCompositorSupport* compositorSupport() {
    return &compositor_support_;
  }

 private:
  webkit::WebCompositorSupportImpl compositor_support_;
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
