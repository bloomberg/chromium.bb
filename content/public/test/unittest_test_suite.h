// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_UNITTEST_TEST_SUITE_H_
#define CONTENT_PUBLIC_TEST_UNITTEST_TEST_SUITE_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class TestSuite;
}

namespace content {

// A special test suite that also initializes WebKit once for all unittests.
// This is useful for two reasons:
// 1. It allows the use of some primitive WebKit data types like WebString.
// 2. Individual unittests should not be initting WebKit on their own, initting
// it here ensures attempts to do so within an individual test will fail.
class UnitTestTestSuite {
 public:
   // Takes ownership of |test_suite|.
  explicit UnitTestTestSuite(base::TestSuite* test_suite);
  ~UnitTestTestSuite();

  int Run();

 private:
  scoped_ptr<base::TestSuite> test_suite_;

  class UnitTestWebKitPlatformSupport;
  scoped_ptr<UnitTestWebKitPlatformSupport> webkit_platform_support_;

  DISALLOW_COPY_AND_ASSIGN(UnitTestTestSuite);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_UNITTEST_TEST_SUITE_H_
