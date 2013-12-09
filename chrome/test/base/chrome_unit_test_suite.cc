// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_unit_test_suite.h"

#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Creates a TestingBrowserProcess for each test.
class ChromeUnitTestSuiteInitializer : public testing::EmptyTestEventListener {
 public:
  ChromeUnitTestSuiteInitializer() {}
  virtual ~ChromeUnitTestSuiteInitializer() {}

  virtual void OnTestStart(const testing::TestInfo& test_info) OVERRIDE {
    TestingBrowserProcess::CreateInstance();
  }

  virtual void OnTestEnd(const testing::TestInfo& test_info) OVERRIDE {
    TestingBrowserProcess::DeleteInstance();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeUnitTestSuiteInitializer);
};

}  // namespace

ChromeUnitTestSuite::ChromeUnitTestSuite(int argc, char** argv)
    : ChromeTestSuite(argc, argv) {}

ChromeUnitTestSuite::~ChromeUnitTestSuite() {}

void ChromeUnitTestSuite::Initialize() {
  // Add an additional listener to do the extra initialization for unit tests.
  // It will be started before the base class listeners and ended after the
  // base class listeners.
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new ChromeUnitTestSuiteInitializer);

  ChromeTestSuite::Initialize();
}
