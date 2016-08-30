// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/sys_info.h"
#include "chrome/test/base/chrome_test_launcher.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "chrome/test/base/mash_browser_tests_main.h"

int main(int argc, char** argv) {
  int exit_code = 0;
  if (RunMashBrowserTests(argc, argv, &exit_code))
    return exit_code;

  int default_jobs = std::max(1, base::SysInfo::NumberOfProcessors() / 2);
  ChromeTestSuiteRunner runner;
  ChromeTestLauncherDelegate delegate(&runner);
  return LaunchChromeTests(default_jobs, &delegate, argc, argv);
}
