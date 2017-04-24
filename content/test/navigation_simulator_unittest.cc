// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/navigation_simulator.h"

#include <string>
#include <tuple>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

enum CancelTime {
  WILL_SEND_REQUEST,
  WILL_REDIRECT_REQUEST,
  WILL_PROCESS_RESPONSE,
  NEVER,
};

enum ResultSynchrony {
  SYNCHRONOUS,
  ASYNCHRONOUS,
};

std::string CancelTimeToString(CancelTime cancel_time) {
  switch (cancel_time) {
    case WILL_SEND_REQUEST:
      return "WILL_SEND_REQUEST";
    case WILL_REDIRECT_REQUEST:
      return "WILL_REDIRECT_REQUEST";
    case WILL_PROCESS_RESPONSE:
      return "WILL_PROCESS_RESPONSE";
    case NEVER:
      return "NEVER";
  }
  NOTREACHED();
  return "";
}

std::string ResultSynchronyToString(ResultSynchrony sync) {
  return sync == SYNCHRONOUS ? "SYNCHRONOUS" : "ASYNCHRONOUS";
}

// TODO(csharrison): Expose this class in the content public API.
class CancellingNavigationThrottle : public NavigationThrottle {
 public:
  CancellingNavigationThrottle(NavigationHandle* handle,
                               CancelTime cancel_time,
                               ResultSynchrony sync)
      : NavigationThrottle(handle),
        cancel_time_(cancel_time),
        sync_(sync),
        weak_ptr_factory_(this) {}

  ~CancellingNavigationThrottle() override {}

  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    return ProcessState(cancel_time_ == WILL_SEND_REQUEST);
  }

  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override {
    return ProcessState(cancel_time_ == WILL_REDIRECT_REQUEST);
  }

  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    return ProcessState(cancel_time_ == WILL_PROCESS_RESPONSE);
  }

  NavigationThrottle::ThrottleCheckResult ProcessState(bool should_cancel) {
    if (sync_ == ASYNCHRONOUS) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&CancellingNavigationThrottle::MaybeCancel,
                     weak_ptr_factory_.GetWeakPtr(), should_cancel));
      return NavigationThrottle::DEFER;
    }
    return should_cancel ? NavigationThrottle::CANCEL
                         : NavigationThrottle::PROCEED;
  }

  void MaybeCancel(bool cancel) {
    if (cancel)
      navigation_handle()->CancelDeferredNavigation(NavigationThrottle::CANCEL);
    else
      navigation_handle()->Resume();
  }

 private:
  const CancelTime cancel_time_;
  const ResultSynchrony sync_;
  base::WeakPtrFactory<CancellingNavigationThrottle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CancellingNavigationThrottle);
};

class NavigationSimulatorTest : public RenderViewHostImplTestHarness,
                                public WebContentsObserver,
                                public testing::WithParamInterface<
                                    std::tuple<CancelTime, ResultSynchrony>> {
 public:
  NavigationSimulatorTest() {}
  ~NavigationSimulatorTest() override {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
    Observe(RenderViewHostImplTestHarness::web_contents());
    std::tie(cancel_time_, sync_) = GetParam();
    simulator_ = NavigationSimulator::CreateRendererInitiated(
        GURL("https://example.test"), main_rfh());
  }

  void TearDown() override {
    EXPECT_TRUE(did_finish_navigation_);
    RenderViewHostImplTestHarness::TearDown();
  }

  void DidStartNavigation(content::NavigationHandle* handle) override {
    handle->RegisterThrottleForTesting(
        base::MakeUnique<CancellingNavigationThrottle>(handle, cancel_time_,
                                                       sync_));
  }

  void DidFinishNavigation(content::NavigationHandle* handle) override {
    did_finish_navigation_ = true;
  }

  CancelTime cancel_time_;
  ResultSynchrony sync_;
  std::unique_ptr<NavigationSimulator> simulator_;
  bool did_finish_navigation_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(NavigationSimulatorTest);
};

// Stress test the navigation simulator by having a navigation throttle cancel
// the navigation at various points in the flow, both synchronously and
// asynchronously.
TEST_P(NavigationSimulatorTest, Cancel) {
  SCOPED_TRACE(::testing::Message()
               << "CancelTime: " << CancelTimeToString(cancel_time_)
               << " ResultSynchrony: " << ResultSynchronyToString(sync_));
  simulator_->Start();
  if (cancel_time_ == WILL_SEND_REQUEST) {
    EXPECT_EQ(NavigationThrottle::CANCEL,
              simulator_->GetLastThrottleCheckResult());
    return;
  }
  EXPECT_EQ(NavigationThrottle::PROCEED,
            simulator_->GetLastThrottleCheckResult());
  simulator_->Redirect(GURL("https://example.redirect"));
  if (cancel_time_ == WILL_REDIRECT_REQUEST) {
    EXPECT_EQ(NavigationThrottle::CANCEL,
              simulator_->GetLastThrottleCheckResult());
    return;
  }
  EXPECT_EQ(NavigationThrottle::PROCEED,
            simulator_->GetLastThrottleCheckResult());
  simulator_->Commit();
  if (cancel_time_ == WILL_PROCESS_RESPONSE) {
    EXPECT_EQ(NavigationThrottle::CANCEL,
              simulator_->GetLastThrottleCheckResult());
    return;
  }
  EXPECT_EQ(NavigationThrottle::PROCEED,
            simulator_->GetLastThrottleCheckResult());
}

INSTANTIATE_TEST_CASE_P(
    CancelMethod,
    NavigationSimulatorTest,
    ::testing::Values(std::make_tuple(WILL_SEND_REQUEST, SYNCHRONOUS),
                      std::make_tuple(WILL_SEND_REQUEST, ASYNCHRONOUS),
                      std::make_tuple(WILL_REDIRECT_REQUEST, SYNCHRONOUS),
                      std::make_tuple(WILL_REDIRECT_REQUEST, ASYNCHRONOUS),
                      std::make_tuple(WILL_PROCESS_RESPONSE, SYNCHRONOUS),
                      std::make_tuple(WILL_PROCESS_RESPONSE, ASYNCHRONOUS),
                      std::make_tuple(NEVER, SYNCHRONOUS),
                      std::make_tuple(NEVER, ASYNCHRONOUS)));

}  // namespace content
