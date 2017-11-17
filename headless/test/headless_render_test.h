// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_TEST_HEADLESS_RENDER_TEST_H_
#define HEADLESS_TEST_HEADLESS_RENDER_TEST_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "headless/public/devtools/domains/network.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_browser_context.h"
#include "headless/test/headless_browser_test.h"

#include "url/gurl.h"

namespace headless {
class HeadlessDevToolsClient;
namespace dom_snapshot {
class GetSnapshotResult;
}  // namespace dom_snapshot

// Base class for tests that render a particular page and verify the output.
class HeadlessRenderTest : public HeadlessAsyncDevTooledBrowserTest,
                           public HeadlessBrowserContext::Observer,
                           public page::ExperimentalObserver,
                           public network::ExperimentalObserver {
 public:
  void RunDevTooledTest() override;

 protected:
  HeadlessRenderTest();
  ~HeadlessRenderTest() override;

  void PostRunAsynchronousTest() override;

  virtual GURL GetPageUrl(HeadlessDevToolsClient* client) = 0;
  virtual void VerifyDom(dom_snapshot::GetSnapshotResult* dom_snapshot) = 0;
  virtual void OnTimeout();
  virtual void OverrideWebPreferences(WebPreferences* preferences);
  void AllDone() { done_called_ = true; }

  void CustomizeHeadlessBrowserContext(
      HeadlessBrowserContext::Builder& builder) override;

  // HeadlessBrowserContext::Observer
  void UrlRequestFailed(net::URLRequest* request,
                        int net_error,
                        bool canceled_by_devtools) override;

  // page::ExperimentalObserver implementation:
  void OnLoadEventFired(const page::LoadEventFiredParams& params) override;

  // network::ExperimentalObserver
  void OnRequestIntercepted(
      const network::RequestInterceptedParams& params) override;

  std::vector<std::unique_ptr<network::RequestInterceptedParams>> requests_;

 private:
  void OnGetDomSnapshotDone(
      std::unique_ptr<dom_snapshot::GetSnapshotResult> result);
  void HandleTimeout();

  bool navigation_requested_ = false;
  bool done_called_ = false;

  base::WeakPtrFactory<HeadlessRenderTest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessRenderTest);
};

}  // namespace headless

#endif  // HEADLESS_TEST_HEADLESS_RENDER_TEST_H_
