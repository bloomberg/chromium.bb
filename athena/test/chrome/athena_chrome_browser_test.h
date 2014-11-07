// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_TEST_CHROME_ATHENA_CHROME_BROWSER_TEST_H_
#define ATHENA_TEST_CHROME_ATHENA_CHROME_BROWSER_TEST_H_

#include "chrome/test/base/in_process_browser_test.h"

namespace base {
class CommandLine;
}

namespace content {
class BrowserContext;
}

namespace athena {

// Base class for athena tests which allows to use WebActivities.
//
// Note: To avoid asynchronous resource manager events, the memory pressure
// callback gets turned off at the beginning to a low memory pressure.
class AthenaChromeBrowserTest : public InProcessBrowserTest {
 public:
  AthenaChromeBrowserTest();
  ~AthenaChromeBrowserTest() override;

 protected:
  // BrowserTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  // Returns the browser context used by the test.
  content::BrowserContext* GetBrowserContext();

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaChromeBrowserTest);
};

}  // namespace athena

#endif  //  ATHENA_TEST_CHROME_ATHENA_CHROME_BROWSER_TEST_H_
