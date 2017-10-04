// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_handle_impl.h"
#include "base/macros.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_side_navigation_test_utils.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"

namespace content {

using ThrottleInsertionCallback =
    base::RepeatingCallback<std::vector<std::unique_ptr<NavigationThrottle>>(
        NavigationHandle*)>;

class ThrottleInserterContentBrowserClient : public TestContentBrowserClient {
 public:
  ThrottleInserterContentBrowserClient(
      const ThrottleInsertionCallback& callback)
      : throttle_insertion_callback_(callback) {}

  std::vector<std::unique_ptr<NavigationThrottle>> CreateThrottlesForNavigation(
      NavigationHandle* navigation_handle) override {
    return throttle_insertion_callback_.Run(navigation_handle);
  }

 private:
  ThrottleInsertionCallback throttle_insertion_callback_;
};

// Test version of a NavigationThrottle that will execute a callback when
// called.
class DeletingNavigationThrottle : public NavigationThrottle {
 public:
  DeletingNavigationThrottle(NavigationHandle* handle,
                             const base::RepeatingClosure& deletion_callback)
      : NavigationThrottle(handle), deletion_callback_(deletion_callback) {}
  ~DeletingNavigationThrottle() override {}

  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    deletion_callback_.Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override {
    deletion_callback_.Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    deletion_callback_.Run();
    return NavigationThrottle::PROCEED;
  }

  const char* GetNameForLogging() override {
    return "DeletingNavigationThrottle";
  }

 private:
  base::RepeatingClosure deletion_callback_;
};

class NavigationHandleImplTest : public RenderViewHostImplTestHarness {
 public:
  NavigationHandleImplTest()
      : was_callback_called_(false),
        callback_result_(NavigationThrottle::DEFER) {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    CreateNavigationHandle();
    EXPECT_EQ(REQUEST_CONTEXT_TYPE_UNSPECIFIED,
              test_handle_->request_context_type_);
    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
  }

  void TearDown() override {
    // Release the |test_handle_| before destroying the WebContents, to match
    // the WebContentsObserverSanityChecker expectations.
    test_handle_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  bool IsDeferringStart() {
    return test_handle_->state() == NavigationHandleImpl::DEFERRING_START;
  }

  bool IsDeferringRedirect() {
    return test_handle_->state() == NavigationHandleImpl::DEFERRING_REDIRECT;
  }

  bool IsDeferringResponse() {
    return test_handle_->state() == NavigationHandleImpl::DEFERRING_RESPONSE;
  }

  bool IsCanceling() {
    return test_handle_->state() == NavigationHandleImpl::CANCELING;
  }

  void Resume() { test_handle_->ResumeInternal(); }

  void CancelDeferredNavigation(
      NavigationThrottle::ThrottleCheckResult result) {
    test_handle_->CancelDeferredNavigationInternal(result);
  }

  // Helper function to call WillStartRequest on |handle|. If this function
  // returns DEFER, |callback_result_| will be set to the actual result of
  // the throttle checks when they are finished.
  void SimulateWillStartRequest() {
    was_callback_called_ = false;
    callback_result_ = NavigationThrottle::DEFER;

    // It's safe to use base::Unretained since the NavigationHandle is owned by
    // the NavigationHandleImplTest.
    test_handle_->WillStartRequest(
        "GET", nullptr, Referrer(), false, ui::PAGE_TRANSITION_LINK, false,
        REQUEST_CONTEXT_TYPE_LOCATION,
        blink::WebMixedContentContextType::kBlockable,
        base::Bind(&NavigationHandleImplTest::UpdateThrottleCheckResult,
                   base::Unretained(this)));
  }

  // Helper function to call WillRedirectRequest on |handle|. If this function
  // returns DEFER, |callback_result_| will be set to the actual result of the
  // throttle checks when they are finished.
  // TODO(clamy): this should also simulate that WillStartRequest was called if
  // it has not been called before.
  void SimulateWillRedirectRequest() {
    was_callback_called_ = false;
    callback_result_ = NavigationThrottle::DEFER;

    // It's safe to use base::Unretained since the NavigationHandle is owned by
    // the NavigationHandleImplTest.
    test_handle_->WillRedirectRequest(
        GURL(), "GET", GURL(), false, scoped_refptr<net::HttpResponseHeaders>(),
        net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1, nullptr,
        base::Bind(&NavigationHandleImplTest::UpdateThrottleCheckResult,
                   base::Unretained(this)));
  }

  // Helper function to call WillProcessResponse on |handle|. If this function
  // returns DEFER, |callback_result_| will be set to the actual result of the
  // throttle checks when they are finished.
  // TODO(clamy): this should also simulate that WillStartRequest was called if
  // it has not been called before.
  void SimulateWillProcessResponse() {
    was_callback_called_ = false;
    callback_result_ = NavigationThrottle::DEFER;

    // It's safe to use base::Unretained since the NavigationHandle is owned
    // by the NavigationHandleImplTest. The ConnectionInfo is different from
    // that sent to WillRedirectRequest to verify that it's correctly plumbed
    // in both cases.
    test_handle_->WillProcessResponse(
        main_test_rfh(), scoped_refptr<net::HttpResponseHeaders>(),
        net::HttpResponseInfo::CONNECTION_INFO_QUIC_35, SSLStatus(),
        GlobalRequestID(), false, false, false, base::Closure(),
        base::Bind(&NavigationHandleImplTest::UpdateThrottleCheckResult,
                   base::Unretained(this)));
  }

  // Returns the handle used in tests.
  NavigationHandleImpl* test_handle() const { return test_handle_.get(); }

  // Whether the callback was called.
  bool was_callback_called() const { return was_callback_called_; }

  // Returns the callback_result.
  NavigationThrottle::ThrottleCheckResult callback_result() const {
    return callback_result_;
  }

  // Creates, register and returns a TestNavigationThrottle that will return
  // |result| on checks.
  TestNavigationThrottle* CreateTestNavigationThrottle(
      NavigationThrottle::ThrottleCheckResult result) {
    TestNavigationThrottle* test_throttle =
        new TestNavigationThrottle(test_handle());
    test_throttle->SetResponseForAllMethods(TestNavigationThrottle::SYNCHRONOUS,
                                            result);
    test_handle()->RegisterThrottleForTesting(
        std::unique_ptr<TestNavigationThrottle>(test_throttle));
    return test_throttle;
  }

  // Creates and register a NavigationThrottle that will delete the
  // NavigationHandle in checks.
  void AddDeletingNavigationThrottle() {
    DCHECK(test_handle_);
    test_handle()->RegisterThrottleForTesting(
        base::MakeUnique<DeletingNavigationThrottle>(
            test_handle(), base::BindRepeating(
                               &NavigationHandleImplTest::ResetNavigationHandle,
                               base::Unretained(this))));
  }

  void CreateNavigationHandle() {
    test_handle_ = NavigationHandleImpl::Create(
        GURL(), std::vector<GURL>(), main_test_rfh()->frame_tree_node(),
        true,   // is_renderer_initiated
        false,  // is_same_document
        base::TimeTicks::Now(), 0,
        false,                  // started_from_context_menu
        CSPDisposition::CHECK,  // should_check_main_world_csp
        false);                 // is_form_submission
  }

 private:
  // The callback provided to NavigationHandleImpl::WillStartRequest and
  // NavigationHandleImpl::WillRedirectRequest during the tests.
  void UpdateThrottleCheckResult(
      NavigationThrottle::ThrottleCheckResult result) {
    callback_result_ = result;
    was_callback_called_ = true;
  }

  void ResetNavigationHandle() { test_handle_ = nullptr; }

  std::unique_ptr<NavigationHandleImpl> test_handle_;
  bool was_callback_called_;
  NavigationThrottle::ThrottleCheckResult callback_result_;
};

// Test harness that automatically inserts a navigation throttle via the content
// browser client.
class NavigationHandleImplThrottleInsertionTest
    : public RenderViewHostImplTestHarness {
 public:
  NavigationHandleImplThrottleInsertionTest() : old_browser_client_(nullptr) {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
    test_browser_client_ =
        base::MakeUnique<ThrottleInserterContentBrowserClient>(
            base::Bind(&NavigationHandleImplThrottleInsertionTest::GetThrottles,
                       base::Unretained(this)));
    old_browser_client_ =
        SetBrowserClientForTesting(test_browser_client_.get());
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_browser_client_);
    RenderViewHostImplTestHarness::TearDown();
  }

  size_t throttles_inserted() const { return throttles_inserted_; }

 private:
  std::vector<std::unique_ptr<NavigationThrottle>> GetThrottles(
      NavigationHandle* handle) {
    auto throttle = base::MakeUnique<TestNavigationThrottle>(handle);
    std::vector<std::unique_ptr<NavigationThrottle>> vec;
    throttles_inserted_++;
    vec.push_back(std::move(throttle));
    return vec;
  }

  std::unique_ptr<ThrottleInserterContentBrowserClient> test_browser_client_;
  ContentBrowserClient* old_browser_client_ = nullptr;

  size_t throttles_inserted_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(NavigationHandleImplThrottleInsertionTest);
};

// Do not insert throttles that correspond to RendererDebugURLs. This aligns
// throttle insertion with WebContentsObserver callbacks.
TEST_F(NavigationHandleImplThrottleInsertionTest,
       RendererDebugURL_DoNotInsert) {
  NavigateAndCommit(GURL("https://example.test/"));
  EXPECT_EQ(1u, throttles_inserted());

  NavigateAndCommit(GURL(kChromeUICrashURL));
  EXPECT_EQ(1u, throttles_inserted());
}

// Checks that the request_context_type is properly set.
// Note: can be extended to cover more internal members.
TEST_F(NavigationHandleImplTest, SimpleDataChecks) {
  SimulateWillStartRequest();
  EXPECT_EQ(REQUEST_CONTEXT_TYPE_LOCATION,
            test_handle()->request_context_type());
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN,
            test_handle()->GetConnectionInfo());

  SimulateWillRedirectRequest();
  EXPECT_EQ(REQUEST_CONTEXT_TYPE_LOCATION,
            test_handle()->request_context_type());
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1,
            test_handle()->GetConnectionInfo());

  SimulateWillProcessResponse();
  EXPECT_EQ(REQUEST_CONTEXT_TYPE_LOCATION,
            test_handle()->request_context_type());
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_QUIC_35,
            test_handle()->GetConnectionInfo());
}

TEST_F(NavigationHandleImplTest, SimpleDataCheckNoRedirect) {
  SimulateWillStartRequest();
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN,
            test_handle()->GetConnectionInfo());

  SimulateWillProcessResponse();
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_QUIC_35,
            test_handle()->GetConnectionInfo());
}

// Checks that a deferred navigation can be properly resumed.
TEST_F(NavigationHandleImplTest, ResumeDeferred) {
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));

  // Simulate WillStartRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillStartRequest();
  EXPECT_TRUE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(1, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));

  // Resume the request. It should no longer be deferred and the callback
  // should have been called.
  Resume();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::PROCEED, callback_result());
  EXPECT_EQ(1, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));

  // Simulate WillRedirectRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillRedirectRequest();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_TRUE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(1, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(1, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));

  // Resume the request. It should no longer be deferred and the callback
  // should have been called.
  Resume();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::PROCEED, callback_result());
  EXPECT_EQ(1, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(1, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));

  // Simulate WillProcessResponse. It will be deferred. The callback should not
  // have been called.
  SimulateWillProcessResponse();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(IsDeferringResponse());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(1, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(1, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(1, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));

  // Resume the request. It should no longer be deferred and the callback should
  // have been called.
  Resume();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::PROCEED, callback_result());
  EXPECT_EQ(1, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(1, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(1, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_TRUE(test_handle()->GetRenderFrameHost());
}

// Checks that a navigation deferred during WillStartRequest can be properly
// cancelled.
TEST_F(NavigationHandleImplTest, CancelDeferredWillStart) {
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));

  // Simulate WillStartRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillStartRequest();
  EXPECT_TRUE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(1, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));

  // Cancel the request. The callback should have been called.
  CancelDeferredNavigation(NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(IsCanceling());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_EQ(1, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
}

// Checks that a navigation deferred during WillRedirectRequest can be properly
// cancelled.
TEST_F(NavigationHandleImplTest, CancelDeferredWillRedirect) {
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));

  // Simulate WillRedirectRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillRedirectRequest();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_TRUE(IsDeferringRedirect());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(1, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));

  // Cancel the request. The callback should have been called.
  CancelDeferredNavigation(NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(IsCanceling());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(1, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
}

// Checks that a navigation deferred can be canceled and not ignored.
TEST_F(NavigationHandleImplTest, CancelDeferredNoIgnore) {
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));

  // Simulate WillRedirectRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillStartRequest();
  EXPECT_TRUE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(1, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));

  // Cancel the request. The callback should have been called with CANCEL, and
  // not CANCEL_AND_IGNORE.
  CancelDeferredNavigation(NavigationThrottle::CANCEL);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(IsCanceling());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL, callback_result());
  EXPECT_EQ(1, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, test_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
}

// Checks that a NavigationThrottle asking to defer followed by a
// NavigationThrottle asking to proceed behave correctly.
TEST_F(NavigationHandleImplTest, DeferThenProceed) {
  TestNavigationThrottle* defer_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  TestNavigationThrottle* proceed_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::PROCEED);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));

  // Simulate WillStartRequest. The request should be deferred. The callback
  // should not have been called. The second throttle should not have been
  // notified.
  SimulateWillStartRequest();
  EXPECT_TRUE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(1, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));

  // Resume the request. It should no longer be deferred and the callback
  // should have been called. The second throttle should have been notified.
  Resume();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::PROCEED, callback_result());
  EXPECT_EQ(1, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(1, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));

  // Simulate WillRedirectRequest. The request should be deferred. The callback
  // should not have been called. The second throttle should not have been
  // notified.
  SimulateWillRedirectRequest();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_TRUE(IsDeferringRedirect());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(1, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(1, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(1, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));

  // Resume the request. It should no longer be deferred and the callback
  // should have been called. The second throttle should have been notified.
  Resume();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::PROCEED, callback_result());
  EXPECT_EQ(1, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(1, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(1, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(1, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
}

// Checks that a NavigationThrottle asking to defer followed by a
// NavigationThrottle asking to cancel behave correctly in WillStartRequest.
TEST_F(NavigationHandleImplTest, DeferThenCancelWillStartRequest) {
  TestNavigationThrottle* defer_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  TestNavigationThrottle* cancel_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));

  // Simulate WillStartRequest. The request should be deferred. The callback
  // should not have been called. The second throttle should not have been
  // notified.
  SimulateWillStartRequest();
  EXPECT_TRUE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(1, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));

  // Resume the request. The callback should have been called. The second
  // throttle should have been notified.
  Resume();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(IsCanceling());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_EQ(1, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(1, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
}

// Checks that a NavigationThrottle asking to defer followed by a
// NavigationThrottle asking to cancel behave correctly in WillRedirectRequest.
TEST_F(NavigationHandleImplTest, DeferThenCancelWillRedirectRequest) {
  TestNavigationThrottle* defer_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  TestNavigationThrottle* cancel_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));

  // Simulate WillRedirectRequest. The request should be deferred. The callback
  // should not have been called. The second throttle should not have been
  // notified.
  SimulateWillRedirectRequest();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_TRUE(IsDeferringRedirect());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(1, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));

  // Resume the request. The callback should have been called. The second
  // throttle should have been notified.
  Resume();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(IsCanceling());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(1, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, defer_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(1, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
}

// Checks that a NavigationThrottle asking to cancel followed by a
// NavigationThrottle asking to proceed behave correctly in WillStartRequest.
// The navigation will be canceled directly, and the second throttle will not
// be called.
TEST_F(NavigationHandleImplTest, CancelThenProceedWillStartRequest) {
  TestNavigationThrottle* cancel_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::CANCEL_AND_IGNORE);
  TestNavigationThrottle* proceed_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::PROCEED);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));

  // Simulate WillStartRequest. The request should not be deferred. The
  // callback should not have been called. The second throttle should not have
  // been notified.
  SimulateWillStartRequest();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_EQ(1, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
}

// Checks that a NavigationThrottle asking to cancel followed by a
// NavigationThrottle asking to proceed behave correctly in WillRedirectRequest.
// The navigation will be canceled directly, and the second throttle will not
// be called.
TEST_F(NavigationHandleImplTest, CancelThenProceedWillRedirectRequest) {
  TestNavigationThrottle* cancel_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::CANCEL_AND_IGNORE);
  TestNavigationThrottle* proceed_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::PROCEED);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));

  // Simulate WillRedirectRequest. The request should not be deferred. The
  // callback should not have been called. The second throttle should not have
  // been notified.
  SimulateWillRedirectRequest();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(1, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
}

// Checks that a NavigationThrottle asking to proceed followed by a
// NavigationThrottle asking to cancel behave correctly in WillProcessResponse.
// Both throttles will be called, and the request will be cancelled.
TEST_F(NavigationHandleImplTest, ProceedThenCancelWillProcessResponse) {
  TestNavigationThrottle* proceed_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::PROCEED);
  TestNavigationThrottle* cancel_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));

  // Simulate WillRedirectRequest. The request should not be deferred. The
  // callback should have been called.
  SimulateWillProcessResponse();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(1, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(1, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
}

// Checks that a NavigationThrottle asking to cancel followed by a
// NavigationThrottle asking to proceed behave correctly in WillProcessResponse.
// The navigation will be canceled directly, and the second throttle will not
// be called.
TEST_F(NavigationHandleImplTest, CancelThenProceedWillProcessResponse) {
  TestNavigationThrottle* cancel_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::CANCEL_AND_IGNORE);
  TestNavigationThrottle* proceed_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::PROCEED);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));

  // Simulate WillProcessResponse. The request should not be deferred. The
  // callback should have been called. The second throttle should not have
  // been notified.
  SimulateWillProcessResponse();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_TRUE(was_callback_called());
  EXPECT_TRUE(IsCanceling());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(1, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
}

// Checks that a NavigationThrottle asking to block the response followed by a
// NavigationThrottle asking to proceed behave correctly in WillProcessResponse.
// The navigation will be canceled directly, and the second throttle will not
// be called.
TEST_F(NavigationHandleImplTest, BlockResponseThenProceedWillProcessResponse) {
  TestNavigationThrottle* cancel_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::BLOCK_RESPONSE);
  TestNavigationThrottle* proceed_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::PROCEED);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));

  // Simulate WillRedirectRequest. The request should not be deferred. The
  // callback should have been called. The second throttle should not have
  // been notified.
  SimulateWillProcessResponse();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_TRUE(was_callback_called());
  EXPECT_TRUE(IsCanceling());
  EXPECT_EQ(NavigationThrottle::BLOCK_RESPONSE, callback_result());
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(1, cancel_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, proceed_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
}

TEST_F(NavigationHandleImplTest, BlockRequestCustomNetError) {
  TestNavigationThrottle* blocked_throttle = CreateTestNavigationThrottle(
      {NavigationThrottle::BLOCK_REQUEST, net::ERR_BLOCKED_BY_ADMINISTRATOR});
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));

  // Simulate WillRedirectRequest. The request should not be deferred. The
  // callback should have been called. The second throttle should not have
  // been notified.
  SimulateWillStartRequest();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_TRUE(was_callback_called());
  EXPECT_TRUE(IsCanceling());
  EXPECT_EQ(NavigationThrottle::BLOCK_REQUEST, callback_result());
  EXPECT_EQ(NavigationThrottle::BLOCK_REQUEST, callback_result().action());
  EXPECT_EQ(net::ERR_BLOCKED_BY_ADMINISTRATOR,
            callback_result().net_error_code());
  EXPECT_FALSE(callback_result().error_page_content().has_value());
  EXPECT_EQ(1, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
}

TEST_F(NavigationHandleImplTest, BlockRequestCustomNetErrorAndErrorHTML) {
  std::string expected_error_page_content("<html><body>test</body></html>");
  TestNavigationThrottle* blocked_throttle = CreateTestNavigationThrottle(
      {NavigationThrottle::BLOCK_REQUEST, net::ERR_BLOCKED_BY_ADMINISTRATOR,
       expected_error_page_content});
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));

  // Simulate WillRedirectRequest. The request should not be deferred. The
  // callback should have been called. The second throttle should not have
  // been notified.
  SimulateWillStartRequest();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_TRUE(was_callback_called());
  EXPECT_TRUE(IsCanceling());
  EXPECT_EQ(NavigationThrottle::BLOCK_REQUEST, callback_result());
  EXPECT_EQ(net::ERR_BLOCKED_BY_ADMINISTRATOR,
            callback_result().net_error_code());
  EXPECT_TRUE(callback_result().error_page_content().has_value());
  EXPECT_EQ(expected_error_page_content,
            callback_result().error_page_content().value());
  EXPECT_EQ(1, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
}

TEST_F(NavigationHandleImplTest, BlockRequestCustomNetErrorInRedirect) {
  // BLOCK_REQUEST on redirect requires PlzNavigate.
  EnableBrowserSideNavigation();
  TestNavigationThrottle* blocked_throttle = CreateTestNavigationThrottle(
      {NavigationThrottle::BLOCK_REQUEST, net::ERR_FILE_NOT_FOUND});
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));

  // Simulate WillRedirectRequest. The request should not be deferred. The
  // callback should have been called. The second throttle should not have
  // been notified.
  SimulateWillRedirectRequest();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_TRUE(was_callback_called());
  EXPECT_TRUE(IsCanceling());
  EXPECT_EQ(NavigationThrottle::BLOCK_REQUEST, callback_result());
  EXPECT_EQ(NavigationThrottle::BLOCK_REQUEST, callback_result().action());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, callback_result().net_error_code());
  EXPECT_FALSE(callback_result().error_page_content().has_value());
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(1, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
}

TEST_F(NavigationHandleImplTest,
       BlockRequestCustomNetErrorAndErrorHTMLInRedirect) {
  // BLOCK_REQUEST on redirect requires PlzNavigate.
  EnableBrowserSideNavigation();
  std::string expected_error_page_content("<html><body>test</body></html>");
  TestNavigationThrottle* blocked_throttle = CreateTestNavigationThrottle(
      {NavigationThrottle::BLOCK_REQUEST, net::ERR_FILE_NOT_FOUND,
       expected_error_page_content});
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));

  // Simulate WillRedirectRequest. The request should not be deferred. The
  // callback should have been called. The second throttle should not have
  // been notified.
  SimulateWillRedirectRequest();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_TRUE(was_callback_called());
  EXPECT_TRUE(IsCanceling());
  EXPECT_EQ(NavigationThrottle::BLOCK_REQUEST, callback_result());
  EXPECT_EQ(NavigationThrottle::BLOCK_REQUEST, callback_result().action());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, callback_result().net_error_code());
  EXPECT_TRUE(callback_result().error_page_content().has_value());
  EXPECT_EQ(expected_error_page_content,
            callback_result().error_page_content().value());
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(1, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, blocked_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
}

TEST_F(NavigationHandleImplTest, BlockResponseCustomNetError) {
  TestNavigationThrottle* block_throttle = CreateTestNavigationThrottle(
      {NavigationThrottle::BLOCK_RESPONSE, net::ERR_FILE_VIRUS_INFECTED});
  EXPECT_EQ(0, block_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, block_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, block_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  // Simulate WillRedirectRequest. The request should not be deferred. The
  // callback should have been called. The second throttle should not have
  // been notified.
  SimulateWillProcessResponse();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_TRUE(was_callback_called());
  EXPECT_TRUE(IsCanceling());
  EXPECT_EQ(NavigationThrottle::BLOCK_RESPONSE, callback_result());
  EXPECT_EQ(NavigationThrottle::BLOCK_RESPONSE, callback_result().action());
  EXPECT_EQ(net::ERR_FILE_VIRUS_INFECTED, callback_result().net_error_code());
  EXPECT_FALSE(callback_result().error_page_content().has_value());
  EXPECT_EQ(0, block_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, block_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(1, block_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
}

TEST_F(NavigationHandleImplTest, BlockResponseCustomNetErrorAndErrorHTML) {
  std::string expected_error_page_content("<html><body>test</body></html>");
  TestNavigationThrottle* block_throttle = CreateTestNavigationThrottle(
      {NavigationThrottle::BLOCK_RESPONSE, net::ERR_FILE_VIRUS_INFECTED,
       expected_error_page_content});
  EXPECT_EQ(0, block_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, block_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(0, block_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  // Simulate WillRedirectRequest. The request should not be deferred. The
  // callback should have been called. The second throttle should not have
  // been notified.
  SimulateWillProcessResponse();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_TRUE(was_callback_called());
  EXPECT_TRUE(IsCanceling());
  EXPECT_EQ(NavigationThrottle::BLOCK_RESPONSE, callback_result());
  EXPECT_EQ(NavigationThrottle::BLOCK_RESPONSE, callback_result().action());
  EXPECT_EQ(net::ERR_FILE_VIRUS_INFECTED, callback_result().net_error_code());
  EXPECT_TRUE(callback_result().error_page_content().has_value());
  EXPECT_EQ(expected_error_page_content,
            callback_result().error_page_content().value());
  EXPECT_EQ(0, block_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_START_REQUEST));
  EXPECT_EQ(0, block_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_REDIRECT_REQUEST));
  EXPECT_EQ(1, block_throttle->GetCallCount(
                   TestNavigationThrottle::WILL_PROCESS_RESPONSE));
}

// Checks that a NavigationHandle can be safely deleted by teh execution of one
// of its NavigationThrottle.
TEST_F(NavigationHandleImplTest, DeletionByNavigationThrottle) {
  // Test deletion in WillStartRequest.
  AddDeletingNavigationThrottle();
  SimulateWillStartRequest();
  EXPECT_EQ(nullptr, test_handle());
  if (IsBrowserSideNavigationEnabled()) {
    EXPECT_FALSE(was_callback_called());
  } else {
    EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  }

  // Test deletion in WillStartRequest after being deferred.
  CreateNavigationHandle();
  CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  AddDeletingNavigationThrottle();
  SimulateWillStartRequest();
  EXPECT_NE(nullptr, test_handle());
  Resume();
  EXPECT_EQ(nullptr, test_handle());
  if (IsBrowserSideNavigationEnabled()) {
    EXPECT_FALSE(was_callback_called());
  } else {
    EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  }

  // Test deletion in WillRedirectRequest.
  CreateNavigationHandle();
  SimulateWillStartRequest();
  AddDeletingNavigationThrottle();
  SimulateWillRedirectRequest();
  EXPECT_EQ(nullptr, test_handle());
  if (IsBrowserSideNavigationEnabled()) {
    EXPECT_FALSE(was_callback_called());
  } else {
    EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  }

  // Test deletion in WillRedirectRequest after being deferred.
  CreateNavigationHandle();
  SimulateWillStartRequest();
  CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  AddDeletingNavigationThrottle();
  SimulateWillRedirectRequest();
  EXPECT_NE(nullptr, test_handle());
  Resume();
  EXPECT_EQ(nullptr, test_handle());
  if (IsBrowserSideNavigationEnabled()) {
    EXPECT_FALSE(was_callback_called());
  } else {
    EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  }

  // Test deletion in WillProcessResponse.
  CreateNavigationHandle();
  SimulateWillStartRequest();
  AddDeletingNavigationThrottle();
  SimulateWillProcessResponse();
  EXPECT_EQ(nullptr, test_handle());
  if (IsBrowserSideNavigationEnabled()) {
    EXPECT_FALSE(was_callback_called());
  } else {
    EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  }

  // Test deletion in WillProcessResponse after being deferred.
  CreateNavigationHandle();
  SimulateWillStartRequest();
  CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  AddDeletingNavigationThrottle();
  SimulateWillProcessResponse();
  EXPECT_NE(nullptr, test_handle());
  Resume();
  EXPECT_EQ(nullptr, test_handle());
  if (IsBrowserSideNavigationEnabled()) {
    EXPECT_FALSE(was_callback_called());
  } else {
    EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  }
}

}  // namespace content
