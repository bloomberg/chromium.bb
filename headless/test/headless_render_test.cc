// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/headless_render_test.h"

#include "headless/public/devtools/domains/dom_snapshot.h"
#include "headless/public/headless_devtools_client.h"
#include "net/url_request/url_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {

HeadlessRenderTest::HeadlessRenderTest() : weak_ptr_factory_(this) {}

HeadlessRenderTest::~HeadlessRenderTest() {}

void HeadlessRenderTest::PostRunAsynchronousTest() {
  // Make sure the test did complete.
  EXPECT_TRUE(done_called_) << "The test did not finish. AllDone() not called.";
}

void HeadlessRenderTest::RunDevTooledTest() {
  EXPECT_TRUE(embedded_test_server()->Start());
  base::RunLoop run_loop;
  devtools_client_->GetPage()->GetExperimental()->AddObserver(this);
  devtools_client_->GetNetwork()->GetExperimental()->AddObserver(this);
  devtools_client_->GetPage()->Enable(run_loop.QuitClosure());
  base::MessageLoop::ScopedNestableTaskAllower nest_loop(
      base::MessageLoop::current());
  run_loop.Run();
  devtools_client_->GetNetwork()->Enable();

  std::unique_ptr<headless::network::RequestPattern> match_all =
      headless::network::RequestPattern::Builder().SetUrlPattern("*").Build();
  std::vector<std::unique_ptr<headless::network::RequestPattern>> patterns;
  patterns.push_back(std::move(match_all));
  devtools_client_->GetNetwork()->GetExperimental()->SetRequestInterception(
      network::SetRequestInterceptionParams::Builder()
          .SetPatterns(std::move(patterns))
          .Build());

  GURL url = GetPageUrl(devtools_client_.get());
  devtools_client_->GetPage()->Navigate(url.spec());
  browser()->BrowserMainThread()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&HeadlessRenderTest::HandleTimeout,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(10));
}

void HeadlessRenderTest::OnTimeout() {
  FAIL() << "Renderer timeout";
}

void HeadlessRenderTest::CustomizeHeadlessBrowserContext(
    HeadlessBrowserContext::Builder& builder) {
  builder.SetOverrideWebPreferencesCallback(
      base::Bind(&HeadlessRenderTest::OverrideWebPreferences,
                 weak_ptr_factory_.GetWeakPtr()));
}

void HeadlessRenderTest::OverrideWebPreferences(WebPreferences* preferences) {
  preferences->hide_scrollbars = true;
  preferences->javascript_enabled = true;
  preferences->autoplay_policy = content::AutoplayPolicy::kUserGestureRequired;
}

void HeadlessRenderTest::UrlRequestFailed(net::URLRequest* request,
                                          int net_error,
                                          bool canceled_by_devtools) {
  if (canceled_by_devtools)
    return;
  FAIL() << "Network request failed: " << net_error << " for "
         << request->url().spec();
}

void HeadlessRenderTest::OnRequestIntercepted(
    const network::RequestInterceptedParams& params) {
  if (params.GetIsNavigationRequest())
    navigation_requested_ = true;
  requests_.push_back(params.Clone());
  // Allow the navigation to proceed.
  devtools_client_->GetNetwork()->GetExperimental()->ContinueInterceptedRequest(
      network::ContinueInterceptedRequestParams::Builder()
          .SetInterceptionId(params.GetInterceptionId())
          .Build());
}

void HeadlessRenderTest::OnLoadEventFired(
    const page::LoadEventFiredParams& params) {
  devtools_client_->GetDOMSnapshot()->GetExperimental()->GetSnapshot(
      dom_snapshot::GetSnapshotParams::Builder()
          .SetComputedStyleWhitelist(std::vector<std::string>())
          .Build(),
      base::Bind(&HeadlessRenderTest::OnGetDomSnapshotDone,
                 weak_ptr_factory_.GetWeakPtr()));
}

void HeadlessRenderTest::OnGetDomSnapshotDone(
    std::unique_ptr<dom_snapshot::GetSnapshotResult> result) {
  EXPECT_TRUE(navigation_requested_);
  VerifyDom(result.get());
  if (done_called_)
    FinishAsynchronousTest();
}

void HeadlessRenderTest::HandleTimeout() {
  if (!done_called_)
    OnTimeout();
  EXPECT_TRUE(done_called_);
  FinishAsynchronousTest();
}

}  // namespace headless
