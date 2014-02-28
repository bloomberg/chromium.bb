// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/sys_info.h"
#include "chrome/test/base/chrome_test_launcher.h"
#include "chrome/test/base/chrome_test_suite.h"

class ChromeBrowserTestSuite : public ChromeTestSuite {
 public:
  ChromeBrowserTestSuite(int argc, char** argv) : ChromeTestSuite(argc, argv) {}
  virtual bool IsBrowserTestSuite() OVERRIDE { return true; }
};

class ChromeBrowserTestSuiteRunner : public ChromeTestSuiteRunner {
 public:
  virtual int RunTestSuite(int argc, char** argv) OVERRIDE {
    return ChromeBrowserTestSuite(argc, argv).Run();
  }
};

int main(int argc, char** argv) {
  int default_jobs = std::max(1, base::SysInfo::NumberOfProcessors() / 2);
  ChromeBrowserTestSuiteRunner runner;
  return LaunchChromeTests(default_jobs, &runner, argc, argv);
}
