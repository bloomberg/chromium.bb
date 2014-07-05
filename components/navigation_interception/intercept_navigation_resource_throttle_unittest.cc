// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "components/navigation_interception/intercept_navigation_resource_throttle.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::ResourceType;
using testing::_;
using testing::Eq;
using testing::Ne;
using testing::Property;
using testing::Return;

namespace navigation_interception {

namespace {

const char kTestUrl[] = "http://www.test.com/";
const char kUnsafeTestUrl[] = "about:crash";

// The MS C++ compiler complains about not being able to resolve which url()
// method (const or non-const) to use if we use the Property matcher to check
// the return value of the NavigationParams::url() method.
// It is possible to suppress the error by specifying the types directly but
// that results in very ugly syntax, which is why these custom matchers are
// used instead.
MATCHER(NavigationParamsUrlIsTest, "") {
  return arg.url() == GURL(kTestUrl);
}

MATCHER(NavigationParamsUrlIsSafe, "") {
  return arg.url() != GURL(kUnsafeTestUrl);
}

}  // namespace


// MockInterceptCallbackReceiver ----------------------------------------------

class MockInterceptCallbackReceiver {
 public:
  MOCK_METHOD2(ShouldIgnoreNavigation,
               bool(content::WebContents* source,
                    const NavigationParams& navigation_params));
};

// MockResourceController -----------------------------------------------------
class MockResourceController : public content::ResourceController {
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

  // ResourceController:
  virtual void Cancel() OVERRIDE {
    NOTREACHED();
  }
  virtual void CancelAndIgnore() OVERRIDE {
    status_ = CANCELLED;
  }
  virtual void CancelWithError(int error_code) OVERRIDE {
    NOTREACHED();
  }
  virtual void Resume() OVERRIDE {
    DCHECK(status_ == UNKNOWN);
    status_ = RESUMED;
  }

 private:
  Status status_;
};

// TestIOThreadState ----------------------------------------------------------

enum RedirectMode {
  REDIRECT_MODE_NO_REDIRECT,
  REDIRECT_MODE_302,
};

class TestIOThreadState {
 public:
  TestIOThreadState(const GURL& url,
                    int render_process_id,
                    int render_frame_id,
                    const std::string& request_method,
                    RedirectMode redirect_mode,
                    MockInterceptCallbackReceiver* callback_receiver)
      : resource_context_(&test_url_request_context_),
        request_(url,
                 net::DEFAULT_PRIORITY,
                 NULL,
                 resource_context_.GetRequestContext()) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    if (render_process_id != MSG_ROUTING_NONE &&
        render_frame_id != MSG_ROUTING_NONE) {
      content::ResourceRequestInfo::AllocateForTesting(
          &request_,
          ResourceType::MAIN_FRAME,
          &resource_context_,
          render_process_id,
          MSG_ROUTING_NONE,
          render_frame_id,
          false);
    }
    throttle_.reset(new InterceptNavigationResourceThrottle(
        &request_,
        base::Bind(&MockInterceptCallbackReceiver::ShouldIgnoreNavigation,
                   base::Unretained(callback_receiver))));
    throttle_->set_controller_for_testing(&throttle_controller_);
    request_.set_method(request_method);

    if (redirect_mode == REDIRECT_MODE_302) {
      net::HttpResponseInfo& response_info =
          const_cast<net::HttpResponseInfo&>(request_.response_info());
      response_info.headers = new net::HttpResponseHeaders(
          "Status: 302 Found\0\0");
    }
  }

  void ThrottleWillStartRequest(bool* defer) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    throttle_->WillStartRequest(defer);
  }

  void ThrottleWillRedirectRequest(const GURL& new_url, bool* defer) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    throttle_->WillRedirectRequest(new_url, defer);
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
  net::TestURLRequestContext test_url_request_context_;
  content::MockResourceContext resource_context_;
  net::URLRequest request_;
  scoped_ptr<InterceptNavigationResourceThrottle> throttle_;
  MockResourceController throttle_controller_;
};

// InterceptNavigationResourceThrottleTest ------------------------------------

class InterceptNavigationResourceThrottleTest
  : public content::RenderViewHostTestHarness {
 public:
  InterceptNavigationResourceThrottleTest()
      : mock_callback_receiver_(new MockInterceptCallbackReceiver()),
        io_thread_state_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    RenderViewHostTestHarness::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    if (web_contents())
      web_contents()->SetDelegate(NULL);

    content::BrowserThread::DeleteSoon(
        content::BrowserThread::IO, FROM_HERE, io_thread_state_);

    RenderViewHostTestHarness::TearDown();
  }

  void SetIOThreadState(TestIOThreadState* io_thread_state) {
    io_thread_state_ = io_thread_state;
  }

  void RunThrottleWillStartRequestOnIOThread(
      const GURL& url,
      const std::string& request_method,
      RedirectMode redirect_mode,
      int render_process_id,
      int render_frame_id,
      bool* defer) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    TestIOThreadState* io_thread_state =
        new TestIOThreadState(url, render_process_id, render_frame_id,
                              request_method, redirect_mode,
                              mock_callback_receiver_.get());

    SetIOThreadState(io_thread_state);

    if (redirect_mode == REDIRECT_MODE_NO_REDIRECT)
      io_thread_state->ThrottleWillStartRequest(defer);
    else
      io_thread_state->ThrottleWillRedirectRequest(url, defer);
  }

 protected:
  enum ShouldIgnoreNavigationCallbackAction {
    IgnoreNavigation,
    DontIgnoreNavigation
  };

  void SetUpWebContentsDelegateAndDrainRunLoop(
      ShouldIgnoreNavigationCallbackAction callback_action,
      bool* defer) {
    ON_CALL(*mock_callback_receiver_, ShouldIgnoreNavigation(_, _))
      .WillByDefault(Return(callback_action == IgnoreNavigation));
    EXPECT_CALL(*mock_callback_receiver_,
                ShouldIgnoreNavigation(web_contents(),
                                       NavigationParamsUrlIsTest()))
      .Times(1);

    content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(
            &InterceptNavigationResourceThrottleTest::
                RunThrottleWillStartRequestOnIOThread,
            base::Unretained(this),
            GURL(kTestUrl),
            "GET",
            REDIRECT_MODE_NO_REDIRECT,
            web_contents()->GetRenderViewHost()->GetProcess()->GetID(),
            web_contents()->GetMainFrame()->GetRoutingID(),
            base::Unretained(defer)));

    // Wait for the request to finish processing.
    base::RunLoop().RunUntilIdle();
  }

  void WaitForPreviouslyScheduledIoThreadWork() {
    base::WaitableEvent io_thread_work_done(true, false);
    content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(
          &base::WaitableEvent::Signal,
          base::Unretained(&io_thread_work_done)));
    io_thread_work_done.Wait();
  }

  scoped_ptr<MockInterceptCallbackReceiver> mock_callback_receiver_;
  TestIOThreadState* io_thread_state_;
};

TEST_F(InterceptNavigationResourceThrottleTest,
       RequestDeferredAndResumedIfNavigationNotIgnored) {
  bool defer = false;
  SetUpWebContentsDelegateAndDrainRunLoop(DontIgnoreNavigation, &defer);

  EXPECT_TRUE(defer);
  ASSERT_TRUE(io_thread_state_);
  EXPECT_TRUE(io_thread_state_->request_resumed());
}

TEST_F(InterceptNavigationResourceThrottleTest,
       RequestDeferredAndCancelledIfNavigationIgnored) {
  bool defer = false;
  SetUpWebContentsDelegateAndDrainRunLoop(IgnoreNavigation, &defer);

  EXPECT_TRUE(defer);
  ASSERT_TRUE(io_thread_state_);
  EXPECT_TRUE(io_thread_state_->request_cancelled());
}

TEST_F(InterceptNavigationResourceThrottleTest,
       NoCallbackMadeIfContentsDeletedWhileThrottleRunning) {
  bool defer = false;

  // The tested scenario is when the WebContents is deleted after the
  // ResourceThrottle has finished processing on the IO thread but before the
  // UI thread callback has been processed.  Since both threads in this test
  // are serviced by one message loop, the post order is the execution order.
  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(_, _))
      .Times(0);

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &InterceptNavigationResourceThrottleTest::
          RunThrottleWillStartRequestOnIOThread,
          base::Unretained(this),
          GURL(kTestUrl),
          "GET",
          REDIRECT_MODE_NO_REDIRECT,
          web_contents()->GetRenderViewHost()->GetProcess()->GetID(),
          web_contents()->GetMainFrame()->GetRoutingID(),
          base::Unretained(&defer)));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &RenderViewHostTestHarness::DeleteContents,
          base::Unretained(this)));

  // The WebContents will now be deleted and only after that will the UI-thread
  // callback posted by the ResourceThrottle be executed.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(defer);
  ASSERT_TRUE(io_thread_state_);
  EXPECT_TRUE(io_thread_state_->request_resumed());
}

TEST_F(InterceptNavigationResourceThrottleTest,
       RequestNotDeferredForRequestNotAssociatedWithARenderView) {
  bool defer = false;

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &InterceptNavigationResourceThrottleTest::
              RunThrottleWillStartRequestOnIOThread,
          base::Unretained(this),
          GURL(kTestUrl),
          "GET",
          REDIRECT_MODE_NO_REDIRECT,
          MSG_ROUTING_NONE,
          MSG_ROUTING_NONE,
          base::Unretained(&defer)));

  // Wait for the request to finish processing.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(defer);
}

TEST_F(InterceptNavigationResourceThrottleTest,
       CallbackCalledWithFilteredUrl) {
  bool defer = false;

  ON_CALL(*mock_callback_receiver_,
          ShouldIgnoreNavigation(_, NavigationParamsUrlIsSafe()))
      .WillByDefault(Return(false));
  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(_, NavigationParamsUrlIsSafe()))
      .Times(1);

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &InterceptNavigationResourceThrottleTest::
              RunThrottleWillStartRequestOnIOThread,
          base::Unretained(this),
          GURL(kUnsafeTestUrl),
          "GET",
          REDIRECT_MODE_NO_REDIRECT,
          web_contents()->GetRenderViewHost()->GetProcess()->GetID(),
          web_contents()->GetMainFrame()->GetRoutingID(),
          base::Unretained(&defer)));

  // Wait for the request to finish processing.
  base::RunLoop().RunUntilIdle();
}

TEST_F(InterceptNavigationResourceThrottleTest,
       CallbackIsPostFalseForGet) {
  bool defer = false;

  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(_, AllOf(
                  NavigationParamsUrlIsSafe(),
                  Property(&NavigationParams::is_post, Eq(false)))))
      .WillOnce(Return(false));

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &InterceptNavigationResourceThrottleTest::
              RunThrottleWillStartRequestOnIOThread,
          base::Unretained(this),
          GURL(kTestUrl),
          "GET",
          REDIRECT_MODE_NO_REDIRECT,
          web_contents()->GetRenderViewHost()->GetProcess()->GetID(),
          web_contents()->GetMainFrame()->GetRoutingID(),
          base::Unretained(&defer)));

  // Wait for the request to finish processing.
  base::RunLoop().RunUntilIdle();
}

TEST_F(InterceptNavigationResourceThrottleTest,
       CallbackIsPostTrueForPost) {
  bool defer = false;

  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(_, AllOf(
                  NavigationParamsUrlIsSafe(),
                  Property(&NavigationParams::is_post, Eq(true)))))
      .WillOnce(Return(false));

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &InterceptNavigationResourceThrottleTest::
              RunThrottleWillStartRequestOnIOThread,
          base::Unretained(this),
          GURL(kTestUrl),
          "POST",
          REDIRECT_MODE_NO_REDIRECT,
          web_contents()->GetRenderViewHost()->GetProcess()->GetID(),
          web_contents()->GetMainFrame()->GetRoutingID(),
          base::Unretained(&defer)));

  // Wait for the request to finish processing.
  base::RunLoop().RunUntilIdle();
}

TEST_F(InterceptNavigationResourceThrottleTest,
       CallbackIsPostFalseForPostConvertedToGetBy302) {
  bool defer = false;

  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(_, AllOf(
                  NavigationParamsUrlIsSafe(),
                  Property(&NavigationParams::is_post, Eq(false)))))
      .WillOnce(Return(false));

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &InterceptNavigationResourceThrottleTest::
              RunThrottleWillStartRequestOnIOThread,
          base::Unretained(this),
          GURL(kTestUrl),
          "POST",
          REDIRECT_MODE_302,
          web_contents()->GetRenderViewHost()->GetProcess()->GetID(),
          web_contents()->GetMainFrame()->GetRoutingID(),
          base::Unretained(&defer)));

  // Wait for the request to finish processing.
  base::RunLoop().RunUntilIdle();
}

}  // namespace navigation_interception
