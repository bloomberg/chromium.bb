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

  FilePath GetTestRootDir();
  // Waits for the test case to finish.
  void WaitForFinish(const FilePath& filename, const int wait_time);
  void RunTest(const FilePath& filename, int timeout);
  void SetUp();
  void TearDown();
  GURL GetTestUrl(const FilePath& filename);
 private:
  void PrepareSrpcHwTest(FilePath test_root_dir);
  void PrepareServerTest(FilePath test_root_dir);
};

#endif  // CHROME_TEST_NACL_NACL_TEST_H_
