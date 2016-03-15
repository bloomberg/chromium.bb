// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_TEST_HEADLESS_BROWSER_TEST_H_
#define HEADLESS_TEST_HEADLESS_BROWSER_TEST_H_

#include "content/public/test/browser_test_base.h"
#include "headless/public/headless_browser.h"

namespace headless {
class HeadlessWebContents;

// Base class for tests which require a full instance of the headless browser.
class HeadlessBrowserTest : public content::BrowserTestBase {
 protected:
  HeadlessBrowserTest();
  ~HeadlessBrowserTest() override;

  // BrowserTestBase:
  void RunTestOnMainThreadLoop() override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // Customize the options used in this test. Note that options which take
  // effect before the message loop has been started (e.g., custom message
  // pumps) cannot be set via this method.
  void SetBrowserOptions(const HeadlessBrowser::Options& options);

  // Navigate to |url| and wait for the document load to complete.
  bool NavigateAndWaitForLoad(HeadlessWebContents* web_contents,
                              const GURL& url);

 protected:
  // Returns the browser for the test.
  HeadlessBrowser* browser() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowserTest);
};

}  // namespace headless

#endif  // HEADLESS_TEST_HEADLESS_BROWSER_TEST_H_
