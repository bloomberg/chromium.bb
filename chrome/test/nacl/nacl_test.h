// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_NACL_NACL_TEST_H_
#define CHROME_TEST_NACL_NACL_TEST_H_
#pragma once

#include "chrome/test/ui/ui_test.h"

class FilePath;

// This class implements integration tests for Native Client.
// For security reasons, Native Client currently loads modules from port 5103
// only, therefore running the test currently requires starting
// a local webserver on this port.
// This limitation will be removed in the future.
// Additional functionality used in this test is implemented in html and JS
// files that are part of NaCl tree.
class NaClTest : public UITest {
 protected:
  NaClTest();
  virtual ~NaClTest();

  // Get the path to the native_client/tests directory, the root of testing
  // data.
  FilePath GetTestRootDir();

  // Waits for a test case (identified by path) to finish.
  void WaitForFinish(const FilePath& filename, int wait_time);

  // Navigate the browser to a given test path, and wait for the test to
  // complete.
  void RunTest(const FilePath& filename, int timeout);

  // Similar to RunTest, but relies on the html file to use the "nexes"
  // property to specify the URLs for all the nexes required to run the test
  // on different architectures.
  void RunMultiarchTest(const FilePath& filename, int timeout);

  // gtest test setup overrides.
  virtual void SetUp();
  virtual void TearDown();

  // Construct a URL for a test file on our local webserver.
  GURL GetTestUrl(const FilePath& filename);

 private:
  // Compute the path to the test binaries (prebuilt NaCL executables).
  FilePath GetTestBinariesDir();
  bool use_x64_nexes_;

  // This test uses an HTML file that lists nexes for different architectures
  // in the "nexes" property
  bool multiarch_test_;

  DISALLOW_COPY_AND_ASSIGN(NaClTest);
};

#endif  // CHROME_TEST_NACL_NACL_TEST_H_
