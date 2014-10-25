// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_TEST_CHROME_ATHENA_BROWSERTEST_H_
#define ATHENA_TEST_CHROME_ATHENA_BROWSERTEST_H_

#include "chrome/test/base/in_process_browser_test.h"

namespace base {
class CommandLine;
}

namespace athena {

// Base class for athena tests which allows to use WebActivities.
//
// Note: To avoid asynchronous resource manager events, the memory pressure
// callback gets turned off at the beginning to a low memory pressure.
class AthenaBrowserTest : public InProcessBrowserTest {
 public:
  AthenaBrowserTest();
  ~AthenaBrowserTest() override;

 protected:
  // BrowserTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaBrowserTest);
};

}  // namespace athena

#endif //  ATHENA_TEST_CHROME_ATHENA_BROWSERTEST_H_

