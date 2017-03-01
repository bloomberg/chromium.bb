// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_TEST_HEADLESS_BROWSER_TEST_H_
#define HEADLESS_TEST_HEADLESS_BROWSER_TEST_H_

#include <memory>
#include <string>

#include "content/public/test/browser_test_base.h"
#include "headless/public/devtools/domains/network.h"
#include "headless/public/devtools/domains/page.h"
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

// A utility class for asynchronously observing load events.
class LoadObserver : public page::Observer, public network::Observer {
 public:
  LoadObserver(HeadlessDevToolsClient* devtools_client, base::Closure callback);
  ~LoadObserver() override;

  // page::Observer implementation:
  void OnLoadEventFired(const page::LoadEventFiredParams& params) override;

  // network::Observer implementation:
  void OnResponseReceived(
      const network::ResponseReceivedParams& params) override;

  bool navigation_succeeded() const { return navigation_succeeded_; }

 private:
  base::Closure callback_;
  HeadlessDevToolsClient* devtools_client_;  // Not owned.

  bool navigation_succeeded_;

  DISALLOW_COPY_AND_ASSIGN(LoadObserver);
};

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

  // Returns the options used by the browser. Modify with caution, since some
  // options only take effect if they were set before browser creation.
  HeadlessBrowser::Options* options() const;

 private:
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowserTest);
};

#define HEADLESS_ASYNC_DEVTOOLED_TEST_F(TEST_FIXTURE_NAME)               \
  IN_PROC_BROWSER_TEST_F(TEST_FIXTURE_NAME, RunAsyncTest) { RunTest(); } \
  class AsyncHeadlessBrowserTestNeedsSemicolon##TEST_FIXTURE_NAME {}

#define HEADLESS_ASYNC_DEVTOOLED_TEST_P(TEST_FIXTURE_NAME)               \
  IN_PROC_BROWSER_TEST_P(TEST_FIXTURE_NAME, RunAsyncTest) { RunTest(); } \
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
  void RenderProcessExited(base::TerminationStatus status,
                           int exit_code) override;

  // Implemented by tests and used to send request(s) to DevTools. Subclasses
  // need to ensure that FinishAsynchronousTest() is called after response(s)
  // are processed (e.g. in a callback).
  virtual void RunDevTooledTest() = 0;

  // Returns the protocol handlers to construct the browser with.  By default
  // the map returned is empty.
  virtual ProtocolHandlerMap GetProtocolHandlers();

 protected:
  void RunTest();

  HeadlessBrowserContext* browser_context_;  // Not owned.
  HeadlessWebContents* web_contents_;
  std::unique_ptr<HeadlessDevToolsClient> devtools_client_;
  bool render_process_exited_;
};

}  // namespace headless

#endif  // HEADLESS_TEST_HEADLESS_BROWSER_TEST_H_
