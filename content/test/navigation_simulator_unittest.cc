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
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class NavigationSimulatorTest
    : public RenderViewHostImplTestHarness,
      public WebContentsObserver,
      public testing::WithParamInterface<
          std::tuple<base::Optional<TestNavigationThrottle::ThrottleMethod>,
                     TestNavigationThrottle::ResultSynchrony>> {
 public:
  NavigationSimulatorTest() : weak_ptr_factory_(this) {}
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
    EXPECT_EQ(expect_navigation_to_finish_, did_finish_navigation_);
    RenderViewHostImplTestHarness::TearDown();
  }

  void DidStartNavigation(content::NavigationHandle* handle) override {
    auto throttle = base::MakeUnique<TestNavigationThrottle>(handle);
    throttle->SetCallback(
        TestNavigationThrottle::WILL_FAIL_REQUEST,
        base::BindRepeating(&NavigationSimulatorTest::OnWillFailRequestCalled,
                            base::Unretained(this)));
    if (cancel_time_.has_value()) {
      throttle->SetResponse(cancel_time_.value(), sync_,
                            NavigationThrottle::CANCEL);
    }
    handle->RegisterThrottleForTesting(
        std::unique_ptr<TestNavigationThrottle>(std::move(throttle)));
  }

  void DidFinishNavigation(content::NavigationHandle* handle) override {
    did_finish_navigation_ = true;
  }

  void OnWillFailRequestCalled() { will_fail_request_called_ = true; }

  base::Optional<TestNavigationThrottle::ThrottleMethod> cancel_time_;
  TestNavigationThrottle::ResultSynchrony sync_;
  std::unique_ptr<NavigationSimulator> simulator_;
  bool expect_navigation_to_finish_ = true;
  bool did_finish_navigation_ = false;
  bool will_fail_request_called_ = false;
  base::WeakPtrFactory<NavigationSimulatorTest> weak_ptr_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NavigationSimulatorTest);
};

// Stress test the navigation simulator by having a navigation throttle cancel
// the navigation at various points in the flow, both synchronously and
// asynchronously.
TEST_P(NavigationSimulatorTest, Cancel) {
  SCOPED_TRACE(::testing::Message()
               << "CancelTime: "
               << (cancel_time_.has_value() ? cancel_time_.value() : -1)
               << " ResultSynchrony: " << sync_);
  simulator_->Start();
  if (cancel_time_.has_value() &&
      cancel_time_.value() == TestNavigationThrottle::WILL_START_REQUEST) {
    EXPECT_EQ(NavigationThrottle::CANCEL,
              simulator_->GetLastThrottleCheckResult());
    return;
  }
  EXPECT_EQ(NavigationThrottle::PROCEED,
            simulator_->GetLastThrottleCheckResult());
  simulator_->Redirect(GURL("https://example.redirect"));
  if (cancel_time_.has_value() &&
      cancel_time_.value() == TestNavigationThrottle::WILL_REDIRECT_REQUEST) {
    EXPECT_EQ(NavigationThrottle::CANCEL,
              simulator_->GetLastThrottleCheckResult());
    return;
  }
  EXPECT_EQ(NavigationThrottle::PROCEED,
            simulator_->GetLastThrottleCheckResult());
  simulator_->Commit();
  if (cancel_time_.has_value() &&
      cancel_time_.value() == TestNavigationThrottle::WILL_PROCESS_RESPONSE) {
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
    ::testing::Combine(
        ::testing::Values(TestNavigationThrottle::WILL_START_REQUEST,
                          TestNavigationThrottle::WILL_REDIRECT_REQUEST,
                          TestNavigationThrottle::WILL_PROCESS_RESPONSE,
                          base::nullopt),
        ::testing::Values(TestNavigationThrottle::SYNCHRONOUS,
                          TestNavigationThrottle::ASYNCHRONOUS)));

// Create a version of the test class for parameterized testing.
using NavigationSimulatorTestCancelFail = NavigationSimulatorTest;

// Test canceling the simulated navigation.
TEST_P(NavigationSimulatorTestCancelFail, Fail) {
  simulator_->Start();
  simulator_->Fail(net::ERR_CERT_DATE_INVALID);
  if (IsBrowserSideNavigationEnabled()) {
    EXPECT_TRUE(will_fail_request_called_);
    EXPECT_EQ(NavigationThrottle::CANCEL,
              simulator_->GetLastThrottleCheckResult());
  } else {
    EXPECT_FALSE(will_fail_request_called_);
    expect_navigation_to_finish_ = false;
  }
}

INSTANTIATE_TEST_CASE_P(
    Fail,
    NavigationSimulatorTestCancelFail,
    ::testing::Combine(
        ::testing::Values(TestNavigationThrottle::WILL_FAIL_REQUEST),
        ::testing::Values(TestNavigationThrottle::SYNCHRONOUS,
                          TestNavigationThrottle::ASYNCHRONOUS)));

// Create a version of the test class for parameterized testing.
using NavigationSimulatorTestCancelFailErrAborted = NavigationSimulatorTest;

// Test canceling the simulated navigation with net::ERR_ABORTED, which should
// not call WillFailRequest on the throttle.
TEST_P(NavigationSimulatorTestCancelFailErrAborted, Fail) {
  simulator_->Start();
  simulator_->Fail(net::ERR_ABORTED);
  EXPECT_FALSE(will_fail_request_called_);
}

INSTANTIATE_TEST_CASE_P(
    Fail,
    NavigationSimulatorTestCancelFailErrAborted,
    ::testing::Combine(
        ::testing::Values(TestNavigationThrottle::WILL_FAIL_REQUEST),
        ::testing::Values(TestNavigationThrottle::SYNCHRONOUS,
                          TestNavigationThrottle::ASYNCHRONOUS)));

}  // namespace content
