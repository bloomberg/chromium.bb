// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/unittest_test_suite.h"

#include "base/logging.h"
#include "base/test/test_suite.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebKitPlatformSupport.h"

// A stubbed out WebKit platform support impl.
class UnitTestTestSuite::UnitTestWebKitPlatformSupport
    : public WebKit::WebKitPlatformSupport {
 public:
  UnitTestWebKitPlatformSupport() {}
  virtual ~UnitTestWebKitPlatformSupport() {}
  virtual void cryptographicallyRandomValues(unsigned char* buffer,
                                             size_t length) OVERRIDE {
    memset(buffer, 0, length);
  }
};

UnitTestTestSuite::UnitTestTestSuite(base::TestSuite* test_suite)
    : test_suite_(test_suite) {
  DCHECK(test_suite);
  webkit_platform_support_.reset(new UnitTestWebKitPlatformSupport);
  WebKit::initialize(webkit_platform_support_.get());
}

UnitTestTestSuite::~UnitTestTestSuite() {
  WebKit::shutdown();
}

int UnitTestTestSuite::Run() {
  return test_suite_->Run();
}
