// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_CLD_DATA_HARNESS_H_
#define CHROME_BROWSER_TRANSLATE_CLD_DATA_HARNESS_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"

namespace test {

// A utility class that sets up CLD dynamic data upon calling Init() and cleans
// it up when destroyed. Note that the Init() method will also configure the
// CLD data source using translate::CldDataSource::Set(), overriding any
// previously-set value unconditionally!
//
// Test data lives under: src/chrome/test/data/cld2_component
//
// This class is intended to be instantiated within IN_PROC_BROWSER_TEST_F
// test fixtures; it uses ASSERT macros for correctness, so that tests will
// fail gracefully in error conditions. Test code should generally use a
// CldDataHarnessFactory to create CldDataHarness objects since this allows
// the tests to run with whatever configuration is appropriate for the platform;
// If that's not enough power, the testing code can set the factory itself.
//
// Sample usage:
//
//   IN_PROC_BROWSER_TEST_F(BrowserTest, PageLanguageDetection) {
//     std::unique_ptr<test::CldDataHarness> cld_data_scope =
//       test::CldDataHarnessFactory::Get()->CreateCldDataHarness();
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
//       cld_data_scope(
//         test::CldDataHarnessFactory::Get->CreateCldDataHarness()) {
//       // (your additional setup code here)
//     }
//     void SetUpOnMainThread() override {
//       cld_data_scope->Init();
//       InProcessBrowserTest::SetUpOnMainThread();
//     }
//    private:
//     std::unique_ptr<test::CldDataHarness> cld_data_scope;
//   };
//
class CldDataHarness {
 public:
  CldDataHarness() {}

  // Reverses the work done by the Init() method: any files and/or directories
  // that would be created by Init() (whether it was called or not) are
  // immediately deleted.
  // If dynamic data is not currently available for any reason, this method has
  // no effect.
  // The default implementation does nothing.
  virtual ~CldDataHarness() {}

  // Call this method, wrapping it in ASSERT_NO_FATAL_FAILURE, to initialize
  // the harness and trigger test failure if initialization fails.
  // IMPORTANT: This method will unconditionally set the CLD data source using
  // translate::CldDataSource::Set(...). Any previously-configured CLD data
  // source will be lost. This helps ensure a consistent test environment where
  // the configured data source matches definitely matches the harness.
  virtual void Init() {}

  // Create and return a new instance of a data harness whose Init() method
  // will configure the "static" CldDataSource.
  static std::unique_ptr<CldDataHarness> CreateStaticDataHarness();

  // Create and return a new instance of a data harness whose Init() method
  // will configure the "standalone" CldDataSource.
  // Unlike NONE() and STATIC(), this data hardness will perform work to allow
  // CLD to load data from a file.
  static std::unique_ptr<CldDataHarness> CreateStandaloneDataHarness();

  // Create and return a new instance of a data harness whose Init() method
  // will configure the "component" CldDataSource.
  // Unlike NONE() and STATIC(), this data hardness will perform work to allow
  // CLD to load data from a file.
  static std::unique_ptr<CldDataHarness> CreateComponentDataHarness();

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

 private:
  DISALLOW_COPY_AND_ASSIGN(CldDataHarness);
};

}  // namespace test

#endif  // CHROME_BROWSER_TRANSLATE_CLD_DATA_HARNESS_H_
