// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_TEST_HEADLESS_BROWSER_TEST_H_
#define HEADLESS_TEST_HEADLESS_BROWSER_TEST_H_

#include "content/public/test/browser_test_base.h"

namespace headless {
class HeadlessBrowser;

// Base class for tests which require a full instance of the headless browser.
class HeadlessBrowserTest : public content::BrowserTestBase {
 protected:
  HeadlessBrowserTest();
  ~HeadlessBrowserTest() override;

  // BrowserTestBase:
  void RunTestOnMainThreadLoop() override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 protected:
  // Returns the browser for the test.
  HeadlessBrowser* browser() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowserTest);
};

}  // namespace headless

#endif  // HEADLESS_TEST_HEADLESS_BROWSER_TEST_H_
