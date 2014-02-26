// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_launcher.h"

int main(int argc, char** argv) {
  // Always run browser perf tests serially - parallel running would be less
  // deterministic and distort perf measurements.
  ChromeTestSuiteRunner runner;
  return LaunchChromeTests(1, &runner, argc, argv);
}
