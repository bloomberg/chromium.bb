// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_IOS_CHROME_UNIT_TEST_SUITE_H_
#define IOS_CHROME_TEST_IOS_CHROME_UNIT_TEST_SUITE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/test/test_suite.h"

// Test suite for unit tests.
class IOSChromeUnitTestSuite : public base::TestSuite {
 public:
  IOSChromeUnitTestSuite(int argc, char** argv);
  ~IOSChromeUnitTestSuite() override;

  // base::TestSuite overrides:
  void Initialize() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(IOSChromeUnitTestSuite);
};

#endif  // IOS_CHROME_TEST_IOS_CHROME_UNIT_TEST_SUITE_H_
