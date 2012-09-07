// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/renderer_host/intercept_navigation_resource_throttle.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread.h"
#include "net/url_request/url_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace content;
using namespace ::testing;

namespace {

const char* kTestUrl = "http://www.test.com/";
const char* kUnsafeTestUrl = "about:crash";

void ContinueTestCase() {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      MessageLoop::QuitClosure());
}

} // namespace

// MockInterceptCallbackReceiver ----------------------------------------------

class MockInterceptCallbackReceiver {
 public:
  MOCK_METHOD4(ShouldIgnoreNavigation, bool(RenderViewHost* source,
                                            const GURL& url,
                                            const content::Referrer& referrer,
                                            bool is_content_initiated));
};

// MockResourceController -----------------------------------------------------
class MockResourceController
    : public content::ResourceController {
 public:
  enum Status {
    UNKNOWN,
    RESUMED,
    CANCELLED
  };

  MockResourceController()
      : status_(UNKNOWN) {
  }

  Status status() const { return status_; }

  // content::ResourceController
  virtual void Cancel() {
    NOTREACHED();
  }
  virtual void CancelAndIgnore() {
    status_ = CANCELLED;
    ContinueTestCase();
  }
  virtual void Resume() {
    DCHECK(status_ == UNKNOWN);
    status_ = RESUMED;
    ContinueTestCase();
  }

 private:
  Status status_;
};

// TestIOThreadState ----------------------------------------------------------

class TestIOThreadState {
 public:
  TestIOThreadState(const GURL& url, int render_process_id, int render_view_id,
                    MockInterceptCallbackReceiver* callback_receiver)
      : request_(url, NULL, resource_context_.GetRequestContext()),
        throttle_(NULL) {
      DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
      if (render_process_id != MSG_ROUTING_NONE &&
          render_view_id != MSG_ROUTING_NONE) {
        ResourceRequestInfo::AllocateForTesting(&request_,
                                                ResourceType::MAIN_FRAME,
                                                &resource_context_,
                                                render_process_id,
                                                render_view_id);
      }
      throttle_.reset(new InterceptNavigationResourceThrottle(
          &request_,
          base::Bind(&MockInterceptCallbackReceiver::ShouldIgnoreNavigation,
                     base::Unretained(callback_receiver))));
      throttle_->set_controller_for_testing(&throttle_controller_);
  }

  void ThrottleWillStartRequest(bool* defer) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    throttle_->WillStartRequest(defer);
  }

  bool request_resumed() const {
    return throttle_controller_.status() ==
        MockResourceController::RESUMED;
  }

  bool request_cancelled() const {
    return throttle_controller_.status() ==
        MockResourceController::CANCELLED;
  }

 private:
  content::MockResourceContext resource_context_;
  net::URLRequest request_;
  scoped_ptr<InterceptNavigationResourceThrottle> throttle_;
  MockResourceController throttle_controller_;
};

// InterceptNavigationResourceThrottleTest ------------------------------------

class InterceptNavigationResourceThrottleTest
  : public ChromeRenderViewHostTestHarness {
 public:
  InterceptNavigationResourceThrottleTest()
      : mock_callback_receiver_(new MockInterceptCallbackReceiver()),
        ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO),
        io_thread_state_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();

    io_thread_.StartIOThread();
  }

  virtual void TearDown() OVERRIDE {
    web_contents()->SetDelegate(NULL);
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&base::DeletePointer<TestIOThreadState>, io_thread_state_));

    RenderViewHostTestHarness::TearDown();
  }

  void SetIOThreadState(TestIOThreadState* io_thread_state) {
    io_thread_state_ = io_thread_state;
  }

  void RunThrottleWillStartRequestOnIOThread(
      const GURL& url,
      int render_process_id,
      int render_view_id,
      bool* defer) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    TestIOThreadState* io_thread_state =
        new TestIOThreadState(url, render_process_id, render_view_id,
                              mock_callback_receiver_.get());

    SetIOThreadState(io_thread_state);
    io_thread_state->ThrottleWillStartRequest(defer);

    if (!*defer) {
      ContinueTestCase();
    }
  }

 protected:
  enum ShouldIgnoreNavigationCallbackAction {
    IgnoreNavigation,
    DontIgnoreNavigation
  };

  void SetUpWebContentsDelegateAndRunMessageLoop(
      ShouldIgnoreNavigationCallbackAction callback_action,
      bool* defer) {

    ON_CALL(*mock_callback_receiver_,
            ShouldIgnoreNavigation(_, _, _, _))
      .WillByDefault(Return(callback_action == IgnoreNavigation));
    EXPECT_CALL(*mock_callback_receiver_,
                ShouldIgnoreNavigation(rvh(), Eq(GURL(kTestUrl)), _, _))
      .Times(1);

    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(
            &InterceptNavigationResourceThrottleTest::
                RunThrottleWillStartRequestOnIOThread,
            base::Unretained(this),
            GURL(kTestUrl),
            web_contents()->GetRenderViewHost()->GetProcess()->GetID(),
            web_contents()->GetRenderViewHost()->GetRoutingID(),
            base::Unretained(defer)));

    // Wait for the request to finish processing.
    message_loop_.Run();
  }

  scoped_ptr<MockInterceptCallbackReceiver> mock_callback_receiver_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  TestIOThreadState* io_thread_state_;
};

TEST_F(InterceptNavigationResourceThrottleTest,
       RequestDeferredAndResumedIfNavigationNotIgnored) {
  bool defer = false;
  SetUpWebContentsDelegateAndRunMessageLoop(DontIgnoreNavigation, &defer);

  EXPECT_TRUE(defer);
  EXPECT_TRUE(io_thread_state_);
  EXPECT_TRUE(io_thread_state_->request_resumed());
}

TEST_F(InterceptNavigationResourceThrottleTest,
       RequestDeferredAndCancelledIfNavigationIgnored) {
  bool defer = false;
  SetUpWebContentsDelegateAndRunMessageLoop(IgnoreNavigation, &defer);

  EXPECT_TRUE(defer);
  EXPECT_TRUE(io_thread_state_);
  EXPECT_TRUE(io_thread_state_->request_cancelled());
}

TEST_F(InterceptNavigationResourceThrottleTest,
       RequestNotDeferredForRequestNotAssociatedWithARenderView) {
  bool defer = false;

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &InterceptNavigationResourceThrottleTest::
              RunThrottleWillStartRequestOnIOThread,
          base::Unretained(this),
          GURL(kTestUrl),
          MSG_ROUTING_NONE,
          MSG_ROUTING_NONE,
          base::Unretained(&defer)));

  // Wait for the request to finish processing.
  message_loop_.Run();

  EXPECT_FALSE(defer);
}

TEST_F(InterceptNavigationResourceThrottleTest,
       CallbackCalledWithFilteredUrl) {
  bool defer = false;

  ON_CALL(*mock_callback_receiver_,
          ShouldIgnoreNavigation(_, Ne(GURL(kUnsafeTestUrl)), _, _))
      .WillByDefault(Return(false));
  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(_, Ne(GURL(kUnsafeTestUrl)), _, _))
      .Times(1);

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &InterceptNavigationResourceThrottleTest::
              RunThrottleWillStartRequestOnIOThread,
          base::Unretained(this),
          GURL(kUnsafeTestUrl),
          web_contents()->GetRenderViewHost()->GetProcess()->GetID(),
          web_contents()->GetRenderViewHost()->GetRoutingID(),
          base::Unretained(&defer)));

  // Wait for the request to finish processing.
  message_loop_.Run();
}
