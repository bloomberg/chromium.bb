// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_BROWSER_TEST_UTILS_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_BROWSER_TEST_UTILS_H_

#include "base/macros.h"

namespace test {

// A utility class that sets up CLD dynamic data upon calling Init() and cleans
// it up when destroyed.
//
// This class is intended to be instantiated within IN_PROC_BROWSER_TEST_F
// test fixtures; it uses ASSERT macros for correctness, so that tests will
// fail gracefully in error conditions. Sample use:
//
//   #include "chrome/browser/translate/translate_browser_test_utils.h"
//
//   IN_PROC_BROWSER_TEST_F(BrowserTest, PageLanguageDetection) {
//     test::ScopedCLDDynamicDataHarness dynamic_data_scope;
//     ASSERT_NO_FATAL_FAILURE(dynamic_data_scope.Init());
//     // ... your code that depends on language detection goes here
//   }
//
// If you have a lot of tests that need language translation features, you can
// add an instance of the ScopedCLDDynamicDataHarness to your test class'
// private member variables and add the call to Init() into your Setup method.
//
// NB: Test data lives under src/chrome/test/data/cld2_component
class ScopedCLDDynamicDataHarness {
 public:
  // Constructs the object, but does nothing. Call Init() to prepare the
  // harness, and enclose that call in ASSERT_NO_FATAL_FAILURE(...).
  ScopedCLDDynamicDataHarness();

  // Reverses the work done by the constructor: any files and/or directories
  // that would be created by the constructor are immediately and irrevocably
  // deleted.
  // If dynamic data is not currently available for any reason, this method has
  // no net effect on the runtime.
  ~ScopedCLDDynamicDataHarness();

  // Call this method, wrapping it in ASSERT_NO_FATAL_FAILURE, to initialize
  // the harness and trigger test failure of initialization fails.
  void Init();

 private:
  void ClearStandaloneDataFileState();

  DISALLOW_COPY_AND_ASSIGN(ScopedCLDDynamicDataHarness);
};

}  // namespace test

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_BROWSER_TEST_UTILS_H_
