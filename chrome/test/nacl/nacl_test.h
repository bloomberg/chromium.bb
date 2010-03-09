// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_NACL_NACL_TEST_H_
#define CHROME_TEST_NACL_NACL_TEST_H_

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

  // gtest test setup overrides.
  virtual void SetUp();
  virtual void TearDown();

  // Construct a URL for a test file on our local webserver.
  GURL GetTestUrl(const FilePath& filename);

  // Get the timeout used for NaCL tests.
  int NaClTestTimeout();

 private:
  void PrepareSrpcHwTest(FilePath test_root_dir);
  void PrepareServerTest(FilePath test_root_dir);
  void PrepareSrpcBasicTest(FilePath test_root_dir);
  void PrepareSrpcSockAddrTest(FilePath test_root_dir);
  void PrepareSrpcShmTest(FilePath test_root_dir);
  void PrepareSrpcPluginTest(FilePath test_root_dir);
  void PrepareSrpcNrdXferTest(FilePath test_root_dir);

  // Compute the path to the test binaries (prebuilt NaCL executables).
  FilePath GetTestBinariesDir();
};

#endif  // CHROME_TEST_NACL_NACL_TEST_H_
