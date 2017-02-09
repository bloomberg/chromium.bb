// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "headless/app/shell_navigation_request.h"
#include "headless/public/devtools/domains/emulation.h"
#include "headless/public/devtools/domains/inspector.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_web_contents.h"
#include "headless/public/util/deterministic_dispatcher.h"
#include "net/base/file_stream.h"

namespace headless {

// An application which implements a simple headless browser.
class HeadlessShell : public HeadlessWebContents::Observer,
                      emulation::ExperimentalObserver,
                      inspector::ExperimentalObserver,
                      page::ExperimentalObserver {
 public:
  HeadlessShell();
  ~HeadlessShell() override;

  // HeadlessWebContents::Observer implementation:
  void DevToolsTargetReady() override;
  void OnTargetCrashed(const inspector::TargetCrashedParams& params) override;

  // emulation::Observer implementation:
  void OnVirtualTimeBudgetExpired(
      const emulation::VirtualTimeBudgetExpiredParams& params) override;

  // page::Observer implementation:
  void OnLoadEventFired(const page::LoadEventFiredParams& params) override;
  void OnNavigationRequested(
      const headless::page::NavigationRequestedParams& params) override;

  void OnStart(HeadlessBrowser* browser);
  void Shutdown();

  void FetchTimeout();

  void PollReadyState();

  void OnReadyState(std::unique_ptr<runtime::EvaluateResult> result);

  void OnPageReady();

  void FetchDom();

  void OnDomFetched(std::unique_ptr<runtime::EvaluateResult> result);

  void InputExpression();

  void OnExpressionResult(std::unique_ptr<runtime::EvaluateResult> result);

  void CaptureScreenshot();

  void OnScreenshotCaptured(
      std::unique_ptr<page::CaptureScreenshotResult> result);

  void OnScreenshotFileOpened(
      std::unique_ptr<page::CaptureScreenshotResult> result,
      const base::FilePath file_name,
      const int open_result);

  void OnScreenshotFileWritten(const base::FilePath file_name,
                               const int length,
                               const int write_result);

  void OnScreenshotFileClosed(const int close_result);

  bool RemoteDebuggingEnabled() const;

  HeadlessDevToolsClient* devtools_client() const {
    return devtools_client_.get();
  }

 private:
  GURL url_;
  HeadlessBrowser* browser_;  // Not owned.
  std::unique_ptr<HeadlessDevToolsClient> devtools_client_;
  HeadlessWebContents* web_contents_;
  bool processed_page_ready_;
  std::unique_ptr<net::FileStream> screenshot_file_stream_;
  HeadlessBrowserContext* browser_context_;
  std::unique_ptr<DeterministicDispatcher> deterministic_dispatcher_;
  base::WeakPtrFactory<HeadlessShell> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessShell);
};

}  // namespace headless
