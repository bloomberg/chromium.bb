// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/headless_render_test.h"

#include "headless/public/devtools/domains/dom_snapshot.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/util/virtual_time_controller.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {

namespace {
void SetVirtualTimePolicyDoneCallback(
    base::RunLoop* run_loop,
    std::unique_ptr<emulation::SetVirtualTimePolicyResult>) {
  run_loop->Quit();
}
}  // namespace

HeadlessRenderTest::HeadlessRenderTest() : weak_ptr_factory_(this) {}

HeadlessRenderTest::~HeadlessRenderTest() {}

void HeadlessRenderTest::PostRunAsynchronousTest() {
  // Make sure the test did complete.
  EXPECT_EQ(FINISHED, state_) << "The test did not finish.";
}

void HeadlessRenderTest::RunDevTooledTest() {
  http_handler_->SetHeadlessBrowserContext(browser_context_);

  virtual_time_controller_ =
      std::make_unique<VirtualTimeController>(devtools_client_.get());

  devtools_client_->GetPage()->GetExperimental()->AddObserver(this);
  devtools_client_->GetPage()->Enable(Sync());

  GURL url = GetPageUrl(devtools_client_.get());

  // Pause virtual time until we actually start loading content.
  {
    base::RunLoop run_loop;
    devtools_client_->GetEmulation()->GetExperimental()->SetVirtualTimePolicy(
        emulation::SetVirtualTimePolicyParams::Builder()
            .SetPolicy(emulation::VirtualTimePolicy::PAUSE)
            .Build(),
        base::Bind(&SetVirtualTimePolicyDoneCallback, &run_loop));
    base::MessageLoop::ScopedNestableTaskAllower nest_loop(
        base::MessageLoop::current());
    run_loop.Run();
  }

  state_ = STARTING;
  devtools_client_->GetPage()->Navigate(url.spec());
  browser()->BrowserMainThread()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&HeadlessRenderTest::HandleTimeout,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(10));

  // The caller will loop until FinishAsynchronousTest() is called either
  // from OnGetDomSnapshotDone() or from HandleTimeout().
}

void HeadlessRenderTest::OnTimeout() {
  ADD_FAILURE() << "Rendering timed out!";
}

void HeadlessRenderTest::CustomizeHeadlessBrowserContext(
    HeadlessBrowserContext::Builder& builder) {
  builder.SetOverrideWebPreferencesCallback(
      base::Bind(&HeadlessRenderTest::OverrideWebPreferences,
                 weak_ptr_factory_.GetWeakPtr()));
}

ProtocolHandlerMap HeadlessRenderTest::GetProtocolHandlers() {
  ProtocolHandlerMap protocol_handlers;
  std::unique_ptr<TestInMemoryProtocolHandler> http_handler(
      new TestInMemoryProtocolHandler(browser()->BrowserIOThread(), this));
  http_handler_ = http_handler.get();
  protocol_handlers[url::kHttpScheme] = std::move(http_handler);
  return protocol_handlers;
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
  ADD_FAILURE() << "Network request failed: " << net_error << " for "
                << request->url().spec();
}

void HeadlessRenderTest::OnLoadEventFired(const page::LoadEventFiredParams&) {
  CHECK_NE(INIT, state_);
  if (state_ == LOADING || state_ == STARTING) {
    state_ = RENDERING;
  }
}

void HeadlessRenderTest::OnFrameStartedLoading(
    const page::FrameStartedLoadingParams&) {
  CHECK_NE(INIT, state_);
  if (state_ == STARTING) {
    state_ = LOADING;
    virtual_time_controller_->GrantVirtualTimeBudget(
        emulation::VirtualTimePolicy::PAUSE_IF_NETWORK_FETCHES_PENDING, 5000,
        base::Closure(),
        base::Bind(&HeadlessRenderTest::HandleVirtualTimeExhausted,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void HeadlessRenderTest::OnRequest(const GURL& url,
                                   base::Closure complete_request) {
  complete_request.Run();
}

void HeadlessRenderTest::OnPageRenderCompleted() {
  CHECK_GE(state_, LOADING);
  if (state_ >= DONE)
    return;
  state_ = DONE;

  devtools_client_->GetDOMSnapshot()->GetExperimental()->GetSnapshot(
      dom_snapshot::GetSnapshotParams::Builder()
          .SetComputedStyleWhitelist(std::vector<std::string>())
          .Build(),
      base::Bind(&HeadlessRenderTest::OnGetDomSnapshotDone,
                 weak_ptr_factory_.GetWeakPtr()));
}

void HeadlessRenderTest::HandleVirtualTimeExhausted() {
  if (state_ < DONE) {
    OnPageRenderCompleted();
  }
}

void HeadlessRenderTest::OnGetDomSnapshotDone(
    std::unique_ptr<dom_snapshot::GetSnapshotResult> result) {
  CHECK_EQ(DONE, state_);
  state_ = FINISHED;
  FinishAsynchronousTest();
  VerifyDom(result.get());
}

void HeadlessRenderTest::HandleTimeout() {
  if (state_ != FINISHED) {
    FinishAsynchronousTest();
    OnTimeout();
  }
}

}  // namespace headless
