// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_TEST_LAUNCHER_H_
#define CHROME_TEST_BASE_CHROME_TEST_LAUNCHER_H_

#include "base/memory/scoped_ptr.h"

class ChromeTestSuite;

// Allows a test suite to override the TestSuite class used. By default it is an
// instance of ChromeTestSuite.
class ChromeTestSuiteRunner {
 public:
  virtual int RunTestSuite(int argc, char** argv) = 0;
};

// Launches Chrome browser tests. |default_jobs| is number of test jobs to be
// run in parallel, unless overridden from the command line. Returns exit code.
int LaunchChromeTests(int default_jobs,
                      ChromeTestSuiteRunner* runner,
                      int argc,
                      char** argv);

#endif  // CHROME_TEST_BASE_CHROME_TEST_LAUNCHER_H_
