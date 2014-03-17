// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PERF_BROWSER_PERF_TEST_H_
#define CHROME_TEST_PERF_BROWSER_PERF_TEST_H_

#include "chrome/test/base/in_process_browser_test.h"

namespace base {
class CommandLine;
}

class BrowserPerfTest : public InProcessBrowserTest {
 public:
  BrowserPerfTest();
  virtual ~BrowserPerfTest();

  // Set up common browser perf test flags. Typically call down to this if
  // overridden.
  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE;

  // Prints IO performance data for use by perf graphs.
  void PrintIOPerfInfo(const std::string& test_name);

  // Prints memory usage data for use by perf graphs.
  void PrintMemoryUsageInfo(const std::string& test_name);
};

#endif  // CHROME_TEST_PERF_BROWSER_PERF_TEST_H_
