// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_TEST_CHROMECAST_BROWSER_TEST_H_
#define CHROMECAST_BROWSER_TEST_CHROMECAST_BROWSER_TEST_H_

#include <memory>

#include "base/macros.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"

namespace content {
class WebContents;
}

namespace chromecast {
namespace shell {
class ChromecastBrowserTestHelper;

// This test allows for running an entire browser-process lifecycle per unit
// test, using Chromecast's cast_shell. This starts up the shell, runs a test
// case, then shuts down the entire shell.
// Note that this process takes 7-10 seconds per test case on Chromecast, so
// fewer test cases with more assertions are preferable.
class ChromecastBrowserTest : public content::BrowserTestBase {
 protected:
  ChromecastBrowserTest();
  ~ChromecastBrowserTest() override;

  // testing::Test implementation:
  void SetUp() override;

  // BrowserTestBase implementation:
  void RunTestOnMainThreadLoop() override;

 protected:
  std::unique_ptr<ChromecastBrowserTestHelper> helper_;

 private:
  bool setup_called_;

  DISALLOW_COPY_AND_ASSIGN(ChromecastBrowserTest);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_TEST_CHROMECAST_BROWSER_TEST_H_
