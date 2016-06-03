// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_TEST_HEADLESS_BROWSER_TEST_H_
#define HEADLESS_TEST_HEADLESS_BROWSER_TEST_H_

#include <memory>
#include <string>

#include "content/public/test/browser_test_base.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_web_contents.h"

namespace base {
class RunLoop;
}

namespace headless {
namespace runtime {
class EvaluateResult;
}
class HeadlessDevToolsClient;

// Base class for tests which require a full instance of the headless browser.
class HeadlessBrowserTest : public content::BrowserTestBase {
 public:
  // Notify that an asynchronous test is now complete and the test runner should
  // exit.
  void FinishAsynchronousTest();

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
  void SetBrowserOptions(HeadlessBrowser::Options options);

  // Run an asynchronous test in a nested run loop. The caller should call
  // FinishAsynchronousTest() to notify that the test should finish.
  void RunAsynchronousTest();

  // Synchronously waits for a tab to finish loading.
  bool WaitForLoad(HeadlessWebContents* web_contents);

  // Synchronously evaluates a script and returns the result.
  std::unique_ptr<runtime::EvaluateResult> EvaluateScript(
      HeadlessWebContents* web_contents,
      const std::string& script);

 protected:
  // Returns the browser for the test.
  HeadlessBrowser* browser() const;

 private:
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowserTest);
};

#define HEADLESS_ASYNC_DEVTOOLED_TEST_F(TEST_FIXTURE_NAME)               \
  IN_PROC_BROWSER_TEST_F(TEST_FIXTURE_NAME, RunAsyncTest) { RunTest(); } \
  class AsyncHeadlessBrowserTestNeedsSemicolon##TEST_FIXTURE_NAME {}

// Base class for tests that require access to a DevToolsClient. Subclasses
// should override the RunDevTooledTest() method, which is called asynchronously
// when the DevToolsClient is ready.
class HeadlessAsyncDevTooledBrowserTest : public HeadlessBrowserTest,
                                          public HeadlessWebContents::Observer {
 public:
  HeadlessAsyncDevTooledBrowserTest();
  ~HeadlessAsyncDevTooledBrowserTest() override;

  // HeadlessWebContentsObserver implementation:
  void DevToolsTargetReady() override;

  // Implemented by tests and used to send request(s) to DevTools. Subclasses
  // need to ensure that FinishAsynchronousTest() is called after response(s)
  // are processed (e.g. in a callback).
  virtual void RunDevTooledTest() = 0;

 protected:
  void RunTest();

  HeadlessWebContents* web_contents_;
  std::unique_ptr<HeadlessDevToolsClient> devtools_client_;
};

}  // namespace headless

#endif  // HEADLESS_TEST_HEADLESS_BROWSER_TEST_H_
