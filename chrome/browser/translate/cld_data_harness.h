// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_CLD_DATA_HARNESS_H_
#define CHROME_BROWSER_TRANSLATE_CLD_DATA_HARNESS_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace test {

// A utility class that sets up CLD dynamic data upon calling Init() and cleans
// it up when destroyed.
// Test data lives under: src/chrome/test/data/cld2_component
//
// This class is intended to be instantiated within IN_PROC_BROWSER_TEST_F
// test fixtures; it uses ASSERT macros for correctness, so that tests will
// fail gracefully in error conditions. Sample use:
//
//   IN_PROC_BROWSER_TEST_F(BrowserTest, PageLanguageDetection) {
//     scoped_ptr<test::CldDataHarness> cld_data_scope =
//       test::CreateCldDataHarness();
//     ASSERT_NO_FATAL_FAILURE(cld_data_scope->Init());
//     // ... your code that depends on language detection goes here
//   }
//
// If you have a lot of tests that need language translation features, you can
// add an instance of the CldDataHarness to your test class' private
// member variables and add the call to Init() into SetUpOnMainThread.
// Sample use:
//
//   class MyTestClass : public InProcessBrowserTest {
//    public:
//     MyTestClass() :
//       cld_data_scope(test::CreateCldDataHarness()) {
//     }
//     virtual void SetUpOnMainThread() OVERRIDE {
//       cld_data_scope->Init();
//       InProcessBrowserTest::SetUpOnMainThread();
//     }
//    private:
//     scoped_ptr<test::CldDataHarness> cld_data_scope;
//   };
//
class CldDataHarness {
 public:
  // Reverses the work done by the Init() method: any files and/or directories
  // that would be created by Init() (whether it was called or not) are
  // immediately deleted.
  // If dynamic data is not currently available for any reason, this method has
  // no effect.
  virtual ~CldDataHarness() {}

  // Call this method, wrapping it in ASSERT_NO_FATAL_FAILURE, to initialize
  // the harness and trigger test failure if initialization fails.
  virtual void Init() = 0;

 protected:
  // Returns the version number of the Component Updater "extension" in the
  // test directory. This generally corresponds the the revision of CLD2 that
  // the data was built from. The version number is also part of the path that
  // would be present at runtime if the component installer was used as the
  // CLD2 data source.
  const base::FilePath::CharType* GetTestDataSourceCrxVersion();

  // Returns the path to the Component Updater "extension" files in the test
  // directory. Within, there is a real copy of the CLD2 dynamic data that can
  // be used in testing scenarios without accessing the network.
  void GetTestDataSourceDirectory(base::FilePath* out_path);
};

// Static factory function that returns a data harness defined by the
// implementation, which must be a subclass of CldDataHarness.
scoped_ptr<CldDataHarness> CreateCldDataHarness();

}  // namespace test

#endif  // CHROME_BROWSER_TRANSLATE_CLD_DATA_HARNESS_H_
