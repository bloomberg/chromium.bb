// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_scheduler.h"

#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/common/resource_messages.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/process_type.h"
#include "content/public/common/resource_type.h"
#include "net/base/host_port_pair.h"
#include "net/base/request_priority.h"
#include "net/http/http_server_properties_impl.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class TestRequestFactory;

const int kChildId = 30;
const int kRouteId = 75;
const int kChildId2 = 43;
const int kRouteId2 = 67;
const int kBackgroundChildId = 35;
const int kBackgroundRouteId = 43;
const int kBackgroundChildId2 = 54;
const int kBackgroundRouteId2 = 82;

class TestRequest : public ResourceController {
 public:
  TestRequest(scoped_ptr<ResourceThrottle> throttle,
              scoped_ptr<net::URLRequest> url_request)
      : started_(false),
        throttle_(throttle.Pass()),
        url_request_(url_request.Pass()) {
    throttle_->set_controller_for_testing(this);
  }
  virtual ~TestRequest() {}

  bool started() const { return started_; }

  void Start() {
    bool deferred = false;
    throttle_->WillStartRequest(&deferred);
    started_ = !deferred;
  }

  virtual void Cancel() OVERRIDE {
    // Alert the scheduler that the request can be deleted.
    throttle_.reset(0);
  }

  const net::URLRequest* url_request() const { return url_request_.get(); }

 protected:
  // ResourceController interface:
  virtual void CancelAndIgnore() OVERRIDE {}
  virtual void CancelWithError(int error_code) OVERRIDE {}
  virtual void Resume() OVERRIDE { started_ = true; }

 private:
  bool started_;
  scoped_ptr<ResourceThrottle> throttle_;
  scoped_ptr<net::URLRequest> url_request_;
};

class CancelingTestRequest : public TestRequest {
 public:
  CancelingTestRequest(scoped_ptr<ResourceThrottle> throttle,
                       scoped_ptr<net::URLRequest> url_request)
      : TestRequest(throttle.Pass(), url_request.Pass()) {}

  void set_request_to_cancel(scoped_ptr<TestRequest> request_to_cancel) {
    request_to_cancel_ = request_to_cancel.Pass();
  }

 private:
  virtual void Resume() OVERRIDE {
    TestRequest::Resume();
    request_to_cancel_.reset();
  }

  scoped_ptr<TestRequest> request_to_cancel_;
};

class FakeResourceContext : public ResourceContext {
 private:
  virtual net::HostResolver* GetHostResolver() OVERRIDE { return NULL; }
  virtual net::URLRequestContext* GetRequestContext() OVERRIDE { return NULL; }
  virtual bool AllowMicAccess(const GURL& origin) OVERRIDE { return false; }
  virtual bool AllowCameraAccess(const GURL& origin) OVERRIDE { return false; }
};

class FakeResourceMessageFilter : public ResourceMessageFilter {
 public:
  FakeResourceMessageFilter(int child_id)
      : ResourceMessageFilter(
          child_id,
          PROCESS_TYPE_RENDERER,
          NULL  /* appcache_service */,
          NULL  /* blob_storage_context */,
          NULL  /* file_system_context */,
          NULL  /* service_worker_context */,
          base::Bind(&FakeResourceMessageFilter::GetContexts,
                     base::Unretained(this))) {
  }

 private:
  virtual ~FakeResourceMessageFilter() {}

  void GetContexts(const ResourceHostMsg_Request& request,
                   ResourceContext** resource_context,
                   net::URLRequestContext** request_context) {
    *resource_context = &context_;
    *request_context = NULL;
  }

  FakeResourceContext context_;
};

class ResourceSchedulerTest : public testing::Test {
 protected:
  ResourceSchedulerTest()
      : next_request_id_(0),
        ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_),
        mock_timer_(new base::MockTimer(true, true)) {
    scheduler_.set_timer_for_testing(scoped_ptr<base::Timer>(mock_timer_));

    // TODO(aiolos): Remove when throttling and coalescing have both landed.
    scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                            false /* should_coalesce */);

    scheduler_.OnClientCreated(kChildId, kRouteId);
    scheduler_.OnVisibilityChanged(kChildId, kRouteId, true);
    scheduler_.OnClientCreated(kBackgroundChildId, kBackgroundRouteId);
    context_.set_http_server_properties(http_server_properties_.GetWeakPtr());
  }

  virtual ~ResourceSchedulerTest() {
    scheduler_.OnClientDeleted(kChildId, kRouteId);
    scheduler_.OnClientDeleted(kBackgroundChildId, kBackgroundRouteId);
  }

  scoped_ptr<net::URLRequest> NewURLRequestWithChildAndRoute(
      const char* url,
      net::RequestPriority priority,
      int child_id,
      int route_id,
      bool is_async) {
    scoped_ptr<net::URLRequest> url_request(
        context_.CreateRequest(GURL(url), priority, NULL, NULL));
    ResourceRequestInfoImpl* info = new ResourceRequestInfoImpl(
        PROCESS_TYPE_RENDERER,                   // process_type
        child_id,                                // child_id
        route_id,                                // route_id
        0,                                       // origin_pid
        ++next_request_id_,                      // request_id
        MSG_ROUTING_NONE,                        // render_frame_id
        false,                                   // is_main_frame
        false,                                   // parent_is_main_frame
        0,                                       // parent_render_frame_id
        RESOURCE_TYPE_SUB_RESOURCE,              // resource_type
        PAGE_TRANSITION_LINK,                    // transition_type
        false,                                   // should_replace_current_entry
        false,                                   // is_download
        false,                                   // is_stream
        true,                                    // allow_download
        false,                                   // has_user_gesture
        false,                                   // enable_load_timing
        blink::WebReferrerPolicyDefault,         // referrer_policy
        blink::WebPageVisibilityStateVisible,    // visibility_state
        NULL,                                    // context
        base::WeakPtr<ResourceMessageFilter>(),  // filter
        is_async);                               // is_async
    info->AssociateWithRequest(url_request.get());
    return url_request.Pass();
  }

  scoped_ptr<net::URLRequest> NewURLRequest(const char* url,
                                            net::RequestPriority priority) {
    return NewURLRequestWithChildAndRoute(
        url, priority, kChildId, kRouteId, true);
  }

  TestRequest* NewRequestWithRoute(const char* url,
                                   net::RequestPriority priority,
                                   int route_id) {
    return NewRequestWithChildAndRoute(url, priority, route_id, kChildId);
  }

  TestRequest* NewRequestWithChildAndRoute(const char* url,
                                           net::RequestPriority priority,
                                           int child_id,
                                           int route_id) {
    return GetNewTestRequest(url, priority, child_id, route_id, true);
  }

  TestRequest* NewRequest(const char* url, net::RequestPriority priority) {
    return NewRequestWithChildAndRoute(url, priority, kChildId, kRouteId);
  }

  TestRequest* NewBackgroundRequest(const char* url,
                                    net::RequestPriority priority) {
    return NewRequestWithChildAndRoute(
        url, priority, kBackgroundChildId, kBackgroundRouteId);
  }

  TestRequest* NewSyncRequest(const char* url, net::RequestPriority priority) {
    return NewSyncRequestWithChildAndRoute(url, priority, kChildId, kRouteId);
  }

  TestRequest* NewBackgroundSyncRequest(const char* url,
                                        net::RequestPriority priority) {
    return NewSyncRequestWithChildAndRoute(
        url, priority, kBackgroundChildId, kBackgroundRouteId);
  }

  TestRequest* NewSyncRequestWithChildAndRoute(const char* url,
                                               net::RequestPriority priority,
                                               int child_id,
                                               int route_id) {
    return GetNewTestRequest(url, priority, child_id, route_id, false);
  }

  TestRequest* GetNewTestRequest(const char* url,
                                 net::RequestPriority priority,
                                 int child_id,
                                 int route_id,
                                 bool is_async) {
    scoped_ptr<net::URLRequest> url_request(NewURLRequestWithChildAndRoute(
        url, priority, child_id, route_id, is_async));
    scoped_ptr<ResourceThrottle> throttle(
        scheduler_.ScheduleRequest(child_id, route_id, url_request.get()));
    TestRequest* request = new TestRequest(throttle.Pass(), url_request.Pass());
    request->Start();
    return request;
  }


  void ChangeRequestPriority(TestRequest* request,
                             net::RequestPriority new_priority,
                             int intra_priority = 0) {
    scoped_refptr<FakeResourceMessageFilter> filter(
        new FakeResourceMessageFilter(kChildId));
    const ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(
        request->url_request());
    const GlobalRequestID& id = info->GetGlobalRequestID();
    ResourceHostMsg_DidChangePriority msg(id.request_id, new_priority,
                                          intra_priority);
    rdh_.OnMessageReceived(msg, filter.get());
  }

  void FireCoalescingTimer() {
    EXPECT_TRUE(mock_timer_->IsRunning());
    mock_timer_->Fire();
  }

  int next_request_id_;
  base::MessageLoopForIO message_loop_;
  BrowserThreadImpl ui_thread_;
  BrowserThreadImpl io_thread_;
  ResourceDispatcherHostImpl rdh_;
  ResourceScheduler scheduler_;
  base::MockTimer* mock_timer_;
  net::HttpServerPropertiesImpl http_server_properties_;
  net::TestURLRequestContext context_;
};

TEST_F(ResourceSchedulerTest, OneIsolatedLowRequest) {
  scoped_ptr<TestRequest> request(NewRequest("http://host/1", net::LOWEST));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, OneLowLoadsUntilIdle) {
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  scoped_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());
  high.reset();
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, OneLowLoadsUntilBodyInserted) {
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  scoped_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());
  high.reset();
  scheduler_.OnWillInsertBody(kChildId, kRouteId);
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, OneLowLoadsUntilCriticalComplete) {
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  scoped_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());
  scheduler_.OnWillInsertBody(kChildId, kRouteId);
  EXPECT_FALSE(low2->started());
  high.reset();
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, OneLowLoadsUntilBodyInsertedExceptSpdy) {
  http_server_properties_.SetSupportsSpdy(
      net::HostPortPair("spdyhost", 443), true);
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low_spdy(
      NewRequest("https://spdyhost/low", net::LOWEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  scoped_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low_spdy->started());
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());
  scheduler_.OnWillInsertBody(kChildId, kRouteId);
  high.reset();
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, NavigationResetsState) {
  scheduler_.OnWillInsertBody(kChildId, kRouteId);
  scheduler_.OnNavigate(kChildId, kRouteId);
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  scoped_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());
}

TEST_F(ResourceSchedulerTest, BackgroundRequestStartsImmediately) {
  const int route_id = 0;  // Indicates a background request.
  scoped_ptr<TestRequest> request(NewRequestWithRoute("http://host/1",
                                                      net::LOWEST, route_id));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, StartMultipleLowRequestsWhenIdle) {
  scoped_ptr<TestRequest> high1(NewRequest("http://host/high1", net::HIGHEST));
  scoped_ptr<TestRequest> high2(NewRequest("http://host/high2", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  scoped_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high1->started());
  EXPECT_TRUE(high2->started());
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());
  high1.reset();
  EXPECT_FALSE(low2->started());
  high2.reset();
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, CancelOtherRequestsWhileResuming) {
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low1(NewRequest("http://host/low1", net::LOWEST));

  scoped_ptr<net::URLRequest> url_request(
      NewURLRequest("http://host/low2", net::LOWEST));
  scoped_ptr<ResourceThrottle> throttle(
      scheduler_.ScheduleRequest(kChildId, kRouteId, url_request.get()));
  scoped_ptr<CancelingTestRequest> low2(new CancelingTestRequest(
      throttle.Pass(), url_request.Pass()));
  low2->Start();

  scoped_ptr<TestRequest> low3(NewRequest("http://host/low3", net::LOWEST));
  low2->set_request_to_cancel(low3.Pass());
  scoped_ptr<TestRequest> low4(NewRequest("http://host/low4", net::LOWEST));

  EXPECT_TRUE(high->started());
  EXPECT_FALSE(low2->started());
  high.reset();
  EXPECT_TRUE(low1->started());
  EXPECT_TRUE(low2->started());
  EXPECT_TRUE(low4->started());
}

TEST_F(ResourceSchedulerTest, LimitedNumberOfDelayableRequestsInFlight) {
  // We only load low priority resources if there's a body.
  scheduler_.OnWillInsertBody(kChildId, kRouteId);

  // Throw in one high priority request to make sure that's not a factor.
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high->started());

  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.
  const int kMaxNumDelayableRequestsPerHost = 6;
  ScopedVector<TestRequest> lows_singlehost;
  // Queue up to the per-host limit (we subtract the current high-pri request).
  for (int i = 0; i < kMaxNumDelayableRequestsPerHost - 1; ++i) {
    string url = "http://host/low" + base::IntToString(i);
    lows_singlehost.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows_singlehost[i]->started());
  }

  scoped_ptr<TestRequest> second_last_singlehost(NewRequest("http://host/last",
                                                            net::LOWEST));
  scoped_ptr<TestRequest> last_singlehost(NewRequest("http://host/s_last",
                                                     net::LOWEST));

  EXPECT_FALSE(second_last_singlehost->started());
  high.reset();
  EXPECT_TRUE(second_last_singlehost->started());
  EXPECT_FALSE(last_singlehost->started());
  lows_singlehost.erase(lows_singlehost.begin());
  EXPECT_TRUE(last_singlehost->started());

  // Queue more requests from different hosts until we reach the total limit.
  int expected_slots_left =
      kMaxNumDelayableRequestsPerClient - kMaxNumDelayableRequestsPerHost;
  EXPECT_GT(expected_slots_left, 0);
  ScopedVector<TestRequest> lows_differenthosts;
  for (int i = 0; i < expected_slots_left; ++i) {
    string url = "http://host" + base::IntToString(i) + "/low";
    lows_differenthosts.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows_differenthosts[i]->started());
  }

  scoped_ptr<TestRequest> last_differenthost(NewRequest("http://host_new/last",
                                                        net::LOWEST));
  EXPECT_FALSE(last_differenthost->started());
}

TEST_F(ResourceSchedulerTest, RaisePriorityAndStart) {
  // Dummies to enforce scheduling.
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/req", net::LOWEST));

  scoped_ptr<TestRequest> request(NewRequest("http://host/req", net::LOWEST));
  EXPECT_FALSE(request->started());

  ChangeRequestPriority(request.get(), net::HIGHEST);
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, RaisePriorityInQueue) {
  // Dummies to enforce scheduling.
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  scoped_ptr<TestRequest> request(NewRequest("http://host/req", net::IDLE));
  scoped_ptr<TestRequest> idle(NewRequest("http://host/idle", net::IDLE));
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  ChangeRequestPriority(request.get(), net::LOWEST);
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.
  ScopedVector<TestRequest> lows;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient - 1; ++i) {
    string url = "http://host/low" + base::IntToString(i);
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }

  scheduler_.OnWillInsertBody(kChildId, kRouteId);
  high.reset();

  EXPECT_TRUE(request->started());
  EXPECT_FALSE(idle->started());
}

TEST_F(ResourceSchedulerTest, LowerPriority) {
  // Dummies to enforce scheduling.
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  scoped_ptr<TestRequest> request(NewRequest("http://host/req", net::LOWEST));
  scoped_ptr<TestRequest> idle(NewRequest("http://host/idle", net::IDLE));
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  ChangeRequestPriority(request.get(), net::IDLE);
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.
  // 2 fewer filler requests: 1 for the "low" dummy at the start, and 1 for the
  // one at the end, which will be tested.
  const int kNumFillerRequests = kMaxNumDelayableRequestsPerClient - 2;
  ScopedVector<TestRequest> lows;
  for (int i = 0; i < kNumFillerRequests; ++i) {
    string url = "http://host" + base::IntToString(i) + "/low";
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }

  scheduler_.OnWillInsertBody(kChildId, kRouteId);
  high.reset();

  EXPECT_FALSE(request->started());
  EXPECT_TRUE(idle->started());
}

TEST_F(ResourceSchedulerTest, ReprioritizedRequestGoesToBackOfQueue) {
  // Dummies to enforce scheduling.
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  scoped_ptr<TestRequest> request(NewRequest("http://host/req", net::LOWEST));
  scoped_ptr<TestRequest> idle(NewRequest("http://host/idle", net::IDLE));
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.
  ScopedVector<TestRequest> lows;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient; ++i) {
    string url = "http://host/low" + base::IntToString(i);
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }

  ChangeRequestPriority(request.get(), net::IDLE);
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  ChangeRequestPriority(request.get(), net::LOWEST);
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  scheduler_.OnWillInsertBody(kChildId, kRouteId);
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());
}

TEST_F(ResourceSchedulerTest, HigherIntraPriorityGoesToFrontOfQueue) {
  // Dummies to enforce scheduling.
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.
  ScopedVector<TestRequest> lows;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient; ++i) {
    string url = "http://host/low" + base::IntToString(i);
    lows.push_back(NewRequest(url.c_str(), net::IDLE));
  }

  scoped_ptr<TestRequest> request(NewRequest("http://host/req", net::IDLE));
  EXPECT_FALSE(request->started());

  ChangeRequestPriority(request.get(), net::IDLE, 1);
  EXPECT_FALSE(request->started());

  scheduler_.OnWillInsertBody(kChildId, kRouteId);
  high.reset();
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, NonHTTPSchedulesImmediately) {
  // Dummies to enforce scheduling.
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  scoped_ptr<TestRequest> request(
      NewRequest("chrome-extension://req", net::LOWEST));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, ActiveLoadingSyncSchedulesImmediately) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  // Dummies to enforce scheduling.
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  scoped_ptr<TestRequest> request(
      NewSyncRequest("http://host/req", net::LOWEST));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, UnthrottledSyncSchedulesImmediately) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  // Dummies to enforce scheduling.
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  scoped_ptr<TestRequest> request(
      NewBackgroundSyncRequest("http://host/req", net::LOWEST));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, SpdyProxySchedulesImmediately) {
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  scoped_ptr<TestRequest> request(NewRequest("http://host/req", net::IDLE));
  EXPECT_FALSE(request->started());

  scheduler_.OnReceivedSpdyProxiedHttpResponse(kChildId, kRouteId);
  EXPECT_TRUE(request->started());

  scoped_ptr<TestRequest> after(NewRequest("http://host/after", net::IDLE));
  EXPECT_TRUE(after->started());
}

TEST_F(ResourceSchedulerTest, NewSpdyHostInDelayableRequests) {
  scheduler_.OnWillInsertBody(kChildId, kRouteId);
  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.

  scoped_ptr<TestRequest> low1_spdy(
      NewRequest("http://spdyhost1:8080/low", net::LOWEST));
  // Cancel a request after we learn the server supports SPDY.
  ScopedVector<TestRequest> lows;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient - 1; ++i) {
    string url = "http://host" + base::IntToString(i) + "/low";
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }
  scoped_ptr<TestRequest> low1(NewRequest("http://host/low", net::LOWEST));
  EXPECT_FALSE(low1->started());
  http_server_properties_.SetSupportsSpdy(
      net::HostPortPair("spdyhost1", 8080), true);
  low1_spdy.reset();
  EXPECT_TRUE(low1->started());

  low1.reset();
  scoped_ptr<TestRequest> low2_spdy(
      NewRequest("http://spdyhost2:8080/low", net::IDLE));
  // Reprioritize a request after we learn the server supports SPDY.
  EXPECT_TRUE(low2_spdy->started());
  http_server_properties_.SetSupportsSpdy(
      net::HostPortPair("spdyhost2", 8080), true);
  ChangeRequestPriority(low2_spdy.get(), net::LOWEST);
  scoped_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, ThrottledClientCreation) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  EXPECT_TRUE(scheduler_.should_throttle());
  scheduler_.OnClientCreated(kBackgroundChildId2, kBackgroundRouteId2);

  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  scheduler_.OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, ActiveClientThrottleUpdateOnLoadingChange) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, false);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
}

TEST_F(ResourceSchedulerTest, CoalesceBackgroundClientOnLoadCompletion) {
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
}

TEST_F(ResourceSchedulerTest, UnthrottleBackgroundClientOnLoadingStarted) {
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));

  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, false);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
}

TEST_F(ResourceSchedulerTest, OneRequestPerThrottledClient) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> request(
      NewBackgroundRequest("http://host/req", net::IDLE));

  EXPECT_TRUE(high->started());
  EXPECT_FALSE(request->started());
}

TEST_F(ResourceSchedulerTest, UnthrottleNewlyVisibleClient) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> request(
      NewBackgroundRequest("http://host/req", net::IDLE));
  EXPECT_FALSE(request->started());

  scheduler_.OnVisibilityChanged(kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, UnthrottleNewlyAudibleClient) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> request(
      NewBackgroundRequest("http://host/req", net::IDLE));
  EXPECT_FALSE(request->started());

  scheduler_.OnAudibilityChanged(kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, VisibleClientStillUnthrottledOnAudabilityChange) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));

  scheduler_.OnAudibilityChanged(kChildId, kRouteId, true);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));

  scheduler_.OnAudibilityChanged(kChildId, kRouteId, false);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
}

TEST_F(ResourceSchedulerTest, AudibleClientStillUnthrottledOnVisabilityChange) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler_.OnVisibilityChanged(kChildId, kRouteId, false);
  scheduler_.OnAudibilityChanged(kChildId, kRouteId, true);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));

  scheduler_.OnVisibilityChanged(kChildId, kRouteId, true);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));

  scheduler_.OnVisibilityChanged(kChildId, kRouteId, false);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
}

TEST_F(ResourceSchedulerTest, ThrottledClientStartsNextHighestPriorityRequest) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scoped_ptr<TestRequest> request(
      NewBackgroundRequest("http://host/req", net::IDLE));
  // Lower priority request started first to test request prioritizaton.
  scoped_ptr<TestRequest> low(
      NewBackgroundRequest("http://host/high", net::IDLE));
  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));

  EXPECT_FALSE(low->started());
  EXPECT_FALSE(high->started());

  // request->CancelRequest();
  request->Cancel();
  EXPECT_TRUE(high->started());
  EXPECT_FALSE(low->started());
}

TEST_F(ResourceSchedulerTest, ThrottledSpdyProxySchedulesImmediately) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> request(
      NewBackgroundRequest("http://host/req", net::IDLE));

  EXPECT_FALSE(request->started());

  scheduler_.OnReceivedSpdyProxiedHttpResponse(kBackgroundChildId,
                                               kBackgroundRouteId);
  EXPECT_TRUE(request->started());

  scoped_ptr<TestRequest> after(
      NewBackgroundRequest("http://host/after", net::IDLE));
  EXPECT_TRUE(after->started());
}

TEST_F(ResourceSchedulerTest, CoalescedClientIssuesNoRequests) {
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> request(
      NewBackgroundRequest("http://host/req", net::IDLE));

  EXPECT_FALSE(high->started());
  EXPECT_FALSE(request->started());

  scheduler_.OnReceivedSpdyProxiedHttpResponse(kBackgroundChildId,
                                               kBackgroundRouteId);
  EXPECT_FALSE(high->started());

  scoped_ptr<TestRequest> after(
      NewBackgroundRequest("http://host/after", net::HIGHEST));
  EXPECT_FALSE(after->started());
}

TEST_F(ResourceSchedulerTest, CoalescedSpdyProxyWaits) {
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> request(
      NewBackgroundRequest("http://host/req", net::IDLE));

  EXPECT_FALSE(request->started());

  scheduler_.OnReceivedSpdyProxiedHttpResponse(kBackgroundChildId,
                                               kBackgroundRouteId);
  EXPECT_FALSE(request->started());

  scoped_ptr<TestRequest> after(
      NewBackgroundRequest("http://host/after", net::IDLE));
  EXPECT_FALSE(after->started());
}

TEST_F(ResourceSchedulerTest, ThrottledNonHTTPSchedulesImmediately) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  // Dummies to enforce scheduling.
  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(
      NewBackgroundRequest("http://host/low", net::LOWEST));

  scoped_ptr<TestRequest> request(
      NewBackgroundRequest("chrome-extension://req", net::LOWEST));
  EXPECT_TRUE(request->started());
  EXPECT_FALSE(low->started());
}

TEST_F(ResourceSchedulerTest, CoalescedNonHTTPSchedulesImmediately) {
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  // Dummies to enforce scheduling.
  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(
      NewBackgroundRequest("http://host/low", net::LOWEST));

  scoped_ptr<TestRequest> request(
      NewBackgroundRequest("chrome-extension://req", net::LOWEST));
  EXPECT_TRUE(request->started());
  EXPECT_FALSE(low->started());
}

TEST_F(ResourceSchedulerTest, ThrottledSyncSchedulesImmediately) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  // Dummies to enforce scheduling.
  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(
      NewBackgroundRequest("http://host/low", net::LOWEST));

  scoped_ptr<TestRequest> request(
      NewBackgroundSyncRequest("http://host/req", net::LOWEST));
  EXPECT_TRUE(request->started());
  EXPECT_FALSE(low->started());
}

TEST_F(ResourceSchedulerTest, CoalescedSyncSchedulesImmediately) {
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  // Dummies to enforce scheduling.
  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(
      NewBackgroundRequest("http://host/low", net::LOWEST));

  scoped_ptr<TestRequest> request(
      NewBackgroundSyncRequest("http://host/req", net::LOWEST));
  EXPECT_TRUE(request->started());
  EXPECT_FALSE(low->started());
  EXPECT_FALSE(high->started());
}

TEST_F(ResourceSchedulerTest, AllBackgroundClientsUnthrottle) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_FALSE(scheduler_.active_clients_loaded());

  scheduler_.OnVisibilityChanged(kChildId, kRouteId, false);
  EXPECT_TRUE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));

  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));

  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, false);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));

  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
}

TEST_F(ResourceSchedulerTest,
       UnloadedClientVisibilityChangedCorrectlyUnthrottles) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler_.OnClientCreated(kChildId2, kRouteId2);
  scheduler_.OnClientCreated(kBackgroundChildId2, kBackgroundRouteId2);
  scheduler_.OnLoadingStateChanged(kChildId2, kRouteId2, true);
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);

  // 1 visible, 3 hidden
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 2 visible, 2 hidden
  scheduler_.OnVisibilityChanged(kChildId2, kRouteId2, true);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible, 3 hidden
  scheduler_.OnVisibilityChanged(kChildId2, kRouteId2, false);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  scheduler_.OnClientDeleted(kChildId2, kRouteId2);
  scheduler_.OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest,
       UnloadedClientAudibilityChangedCorrectlyUnthrottles) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler_.OnClientCreated(kChildId2, kRouteId2);
  scheduler_.OnClientCreated(kBackgroundChildId2, kBackgroundRouteId2);
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  scheduler_.OnVisibilityChanged(kChildId, kRouteId, false);
  scheduler_.OnAudibilityChanged(kChildId, kRouteId, true);

  // 1 audible, 3 hidden
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 2 audible, 2 hidden
  scheduler_.OnAudibilityChanged(kChildId2, kRouteId2, true);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 1 audible, 3 hidden
  scheduler_.OnAudibilityChanged(kChildId2, kRouteId2, false);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  scheduler_.OnClientDeleted(kChildId2, kRouteId2);
  scheduler_.OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest,
       LoadedClientVisibilityChangedCorrectlyUnthrottles) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler_.OnClientCreated(kChildId2, kRouteId2);
  scheduler_.OnClientCreated(kBackgroundChildId2, kBackgroundRouteId2);
  scheduler_.OnLoadingStateChanged(kChildId2, kRouteId2, true);
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  // 1 visible, 3 hidden
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 2 visible, 2 hidden
  scheduler_.OnVisibilityChanged(kChildId2, kRouteId2, true);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible, 3 hidden
  scheduler_.OnVisibilityChanged(kChildId2, kRouteId2, false);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  scheduler_.OnClientDeleted(kChildId2, kRouteId2);
  scheduler_.OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest,
       LoadedClientAudibilityChangedCorrectlyUnthrottles) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler_.OnClientCreated(kChildId2, kRouteId2);
  scheduler_.OnClientCreated(kBackgroundChildId2, kBackgroundRouteId2);
  scheduler_.OnLoadingStateChanged(kChildId2, kRouteId2, true);
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  scheduler_.OnVisibilityChanged(kChildId, kRouteId, false);
  scheduler_.OnAudibilityChanged(kChildId, kRouteId, true);
  // 1 audible, 3 hidden
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 2 audible, 2 hidden
  scheduler_.OnAudibilityChanged(kChildId2, kRouteId2, true);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 1 audible, 3 hidden
  scheduler_.OnAudibilityChanged(kChildId2, kRouteId2, false);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  scheduler_.OnClientDeleted(kChildId2, kRouteId2);
  scheduler_.OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, UnloadedClientBecomesHiddenCorrectlyUnthrottles) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler_.OnClientCreated(kChildId2, kRouteId2);
  scheduler_.OnClientCreated(kBackgroundChildId2, kBackgroundRouteId2);
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);

  // 2 visible, 2 hidden
  scheduler_.OnVisibilityChanged(kChildId2, kRouteId2, true);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible, 3 hidden
  scheduler_.OnVisibilityChanged(kChildId2, kRouteId2, false);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 0 visible, 4 hidden
  scheduler_.OnVisibilityChanged(kChildId, kRouteId, false);
  EXPECT_TRUE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible, 3 hidden
  scheduler_.OnVisibilityChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  scheduler_.OnClientDeleted(kChildId2, kRouteId2);
  scheduler_.OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, UnloadedClientBecomesSilentCorrectlyUnthrottles) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler_.OnClientCreated(kChildId2, kRouteId2);
  scheduler_.OnClientCreated(kBackgroundChildId2, kBackgroundRouteId2);
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  scheduler_.OnAudibilityChanged(kChildId, kRouteId, true);
  scheduler_.OnVisibilityChanged(kChildId, kRouteId, false);
  scheduler_.OnAudibilityChanged(kChildId2, kRouteId2, true);
  // 2 audible, 2 hidden
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 1 audible, 3 hidden
  scheduler_.OnAudibilityChanged(kChildId2, kRouteId2, false);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 0 audible, 4 hidden
  scheduler_.OnAudibilityChanged(kChildId, kRouteId, false);
  EXPECT_TRUE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 1 audible, 3 hidden
  scheduler_.OnAudibilityChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  scheduler_.OnClientDeleted(kChildId2, kRouteId2);
  scheduler_.OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, LoadedClientBecomesHiddenCorrectlyThrottles) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler_.OnClientCreated(kChildId2, kRouteId2);
  scheduler_.OnClientCreated(kBackgroundChildId2, kBackgroundRouteId2);
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  scheduler_.OnLoadingStateChanged(kChildId2, kRouteId2, true);
  scheduler_.OnVisibilityChanged(kChildId2, kRouteId2, true);
  // 2 visible, 2 hidden
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible, 3 hidden
  scheduler_.OnVisibilityChanged(kChildId2, kRouteId2, false);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 0 visible, 4 hidden
  scheduler_.OnVisibilityChanged(kChildId, kRouteId, false);
  EXPECT_TRUE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible, 3 hidden
  scheduler_.OnVisibilityChanged(kChildId2, kRouteId2, true);
  EXPECT_TRUE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  scheduler_.OnClientDeleted(kChildId2, kRouteId2);
  scheduler_.OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, LoadedClientBecomesSilentCorrectlyThrottles) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler_.OnClientCreated(kChildId2, kRouteId2);
  scheduler_.OnClientCreated(kBackgroundChildId2, kBackgroundRouteId2);
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  scheduler_.OnLoadingStateChanged(kChildId2, kRouteId2, true);
  scheduler_.OnVisibilityChanged(kChildId, kRouteId, false);
  scheduler_.OnAudibilityChanged(kChildId, kRouteId, true);
  scheduler_.OnAudibilityChanged(kChildId2, kRouteId2, true);
  // 2 audible, 2 hidden
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 1 audible, 3 hidden
  scheduler_.OnAudibilityChanged(kChildId2, kRouteId2, false);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 0 audible, 4 hidden
  scheduler_.OnAudibilityChanged(kChildId, kRouteId, false);
  EXPECT_TRUE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 1 audible, 3 hidden
  scheduler_.OnAudibilityChanged(kChildId2, kRouteId2, true);
  EXPECT_TRUE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  scheduler_.OnClientDeleted(kChildId2, kRouteId2);
  scheduler_.OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, HiddenLoadedChangesCorrectlyStayThrottled) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler_.OnClientCreated(kChildId2, kRouteId2);
  scheduler_.OnClientCreated(kBackgroundChildId2, kBackgroundRouteId2);

  // 1 visible and 2 hidden loading, 1 visible loaded
  scheduler_.OnVisibilityChanged(kChildId2, kRouteId2, true);
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible and 1 hidden loading, 1 visible and 1 hidden loaded
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible loading, 1 visible and 2 hidden loaded
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible and 1 hidden loading, 1 visible and 1 hidden loaded
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  scheduler_.OnClientDeleted(kChildId2, kRouteId2);
  scheduler_.OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, PartialVisibleClientLoadedDoesNotUnthrottle) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler_.OnClientCreated(kChildId2, kRouteId2);
  scheduler_.OnClientCreated(kBackgroundChildId2, kBackgroundRouteId2);
  scheduler_.OnVisibilityChanged(kChildId2, kRouteId2, true);

  // 2 visible loading, 1 hidden loading, 1 hidden loaded
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible and 1 hidden loaded, 1 visible and 1 hidden loading
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 2 visible loading, 1 hidden loading, 1 hidden loaded
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, false);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  scheduler_.OnClientDeleted(kChildId2, kRouteId2);
  scheduler_.OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, FullVisibleLoadedCorrectlyUnthrottle) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler_.OnClientCreated(kChildId2, kRouteId2);
  scheduler_.OnClientCreated(kBackgroundChildId2, kBackgroundRouteId2);
  scheduler_.OnVisibilityChanged(kChildId2, kRouteId2, true);

  // 1 visible and 1 hidden loaded, 1 visible and 1 hidden loading
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  scheduler_.OnLoadingStateChanged(kChildId2, kRouteId2, true);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(
      NewBackgroundRequest("http://host/low", net::LOWEST));

  EXPECT_TRUE(high->started());
  EXPECT_FALSE(low->started());

  // 2 visible loaded, 1 hidden loading, 1 hidden loaded
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_TRUE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  // kBackgroundClientId unthrottling should unthrottle it's request.
  EXPECT_TRUE(low->started());

  // 1 visible and 1 hidden loaded, 1 visible and 1 hidden loading
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, false);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  scheduler_.OnClientDeleted(kChildId2, kRouteId2);
  scheduler_.OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest,
       ActiveAndLoadingClientDeletedCorrectlyUnthrottle) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler_.OnClientCreated(kChildId2, kRouteId2);
  scheduler_.OnClientCreated(kBackgroundChildId2, kBackgroundRouteId2);
  scheduler_.OnVisibilityChanged(kChildId2, kRouteId2, true);

  // 1 visible and 1 hidden loaded, 1 visible and 1 hidden loading
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId2, kRouteId2));

  // 1 visible loaded, 1 hidden loading, 1 hidden loaded
  scheduler_.OnClientDeleted(kChildId2, kRouteId2);
  EXPECT_TRUE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible and 1 hidden loaded, 1 visible and 1 hidden loading
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, false);
  EXPECT_FALSE(scheduler_.active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));

  scheduler_.OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, CoalescedClientCreationStartsTimer) {
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(mock_timer_->IsRunning());
}

TEST_F(ResourceSchedulerTest, ActiveLoadingClientLoadedAndHiddenStartsTimer) {
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  EXPECT_FALSE(mock_timer_->IsRunning());
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_FALSE(mock_timer_->IsRunning());

  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_FALSE(mock_timer_->IsRunning());

  scheduler_.OnVisibilityChanged(kChildId, kRouteId, false);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(mock_timer_->IsRunning());
}

TEST_F(ResourceSchedulerTest, ActiveLoadingClientHiddenAndLoadedStartsTimer) {
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_FALSE(mock_timer_->IsRunning());

  scheduler_.OnVisibilityChanged(kChildId, kRouteId, false);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_FALSE(mock_timer_->IsRunning());

  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(mock_timer_->IsRunning());
}

TEST_F(ResourceSchedulerTest, CoalescedClientBecomesAudibleStopsTimer) {
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(mock_timer_->IsRunning());

  scheduler_.OnAudibilityChanged(kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_FALSE(mock_timer_->IsRunning());
}

TEST_F(ResourceSchedulerTest, LastCoalescedClientDeletionStopsTimer) {
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler_.OnClientCreated(kBackgroundChildId2, kBackgroundRouteId2);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_TRUE(mock_timer_->IsRunning());

  scheduler_.OnClientDeleted(kBackgroundChildId, kBackgroundRouteId);
  EXPECT_TRUE(mock_timer_->IsRunning());

  scheduler_.OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
  EXPECT_FALSE(mock_timer_->IsRunning());

  // To avoid errors on test tear down.
  scheduler_.OnClientCreated(kBackgroundChildId, kBackgroundRouteId);
}

TEST_F(ResourceSchedulerTest, LastCoalescedClientStartsLoadingStopsTimer) {
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler_.OnClientCreated(kBackgroundChildId2, kBackgroundRouteId2);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_TRUE(mock_timer_->IsRunning());

  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, false);
  EXPECT_TRUE(mock_timer_->IsRunning());

  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, false);
  EXPECT_FALSE(mock_timer_->IsRunning());

  // This is needed to avoid errors on test tear down.
  scheduler_.OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, LastCoalescedClientBecomesVisibleStopsTimer) {
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler_.OnClientCreated(kBackgroundChildId2, kBackgroundRouteId2);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_TRUE(mock_timer_->IsRunning());

  scheduler_.OnVisibilityChanged(kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_TRUE(mock_timer_->IsRunning());

  scheduler_.OnVisibilityChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  EXPECT_FALSE(mock_timer_->IsRunning());

  // To avoid errors on test tear down.
  scheduler_.OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest,
       CoalescedClientBecomesLoadingAndVisibleStopsTimer) {
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(mock_timer_->IsRunning());

  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, false);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_FALSE(mock_timer_->IsRunning());

  scheduler_.OnVisibilityChanged(kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_FALSE(mock_timer_->IsRunning());
}

TEST_F(ResourceSchedulerTest, CoalescedRequestsIssueOnTimer) {
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(scheduler_.active_clients_loaded());

  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(
      NewBackgroundRequest("http://host/low", net::LOWEST));
  EXPECT_FALSE(high->started());
  EXPECT_FALSE(low->started());

  FireCoalescingTimer();

  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low->started());
}

TEST_F(ResourceSchedulerTest, CoalescedRequestsUnthrottleCorrectlyOnTimer) {
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(scheduler_.active_clients_loaded());

  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> high2(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> high3(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> high4(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(
      NewBackgroundRequest("http://host/low", net::LOWEST));
  scoped_ptr<TestRequest> low2(
      NewBackgroundRequest("http://host/low", net::LOWEST));
  scoped_ptr<TestRequest> low3(
      NewBackgroundRequest("http://host/low", net::LOWEST));
  scoped_ptr<TestRequest> low4(
      NewBackgroundRequest("http://host/low", net::LOWEST));

  http_server_properties_.SetSupportsSpdy(net::HostPortPair("spdyhost", 443),
                                          true);
  scoped_ptr<TestRequest> low_spdy(
      NewBackgroundRequest("https://spdyhost/low", net::LOW));
  scoped_ptr<TestRequest> sync_request(
      NewBackgroundSyncRequest("http://host/req", net::LOW));
  scoped_ptr<TestRequest> non_http_request(
      NewBackgroundRequest("chrome-extension://req", net::LOW));

  // Sync requests should issue immediately.
  EXPECT_TRUE(sync_request->started());
  // Non-http(s) requests should issue immediately.
  EXPECT_TRUE(non_http_request->started());
  // Nothing else should issue without a timer fire.
  EXPECT_FALSE(high->started());
  EXPECT_FALSE(high2->started());
  EXPECT_FALSE(high3->started());
  EXPECT_FALSE(high4->started());
  EXPECT_FALSE(low->started());
  EXPECT_FALSE(low2->started());
  EXPECT_FALSE(low3->started());
  EXPECT_FALSE(low4->started());
  EXPECT_FALSE(low_spdy->started());

  FireCoalescingTimer();

  // All high priority requests should issue.
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(high2->started());
  EXPECT_TRUE(high3->started());
  EXPECT_TRUE(high4->started());
  // There should only be one net::LOWEST priority request issued with
  // non-delayable requests in flight.
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());
  EXPECT_FALSE(low3->started());
  EXPECT_FALSE(low4->started());
  // Spdy-Enable requests should issue regardless of priority.
  EXPECT_TRUE(low_spdy->started());
}

TEST_F(ResourceSchedulerTest, CoalescedRequestsWaitForNextTimer) {
  scheduler_.SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler_.OnLoadingStateChanged(kChildId, kRouteId, true);
  scheduler_.OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);

  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler_.GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(scheduler_.active_clients_loaded());

  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  EXPECT_FALSE(high->started());

  FireCoalescingTimer();

  scoped_ptr<TestRequest> high2(
      NewBackgroundRequest("http://host/high2", net::HIGHEST));
  scoped_ptr<TestRequest> low(
      NewBackgroundRequest("http://host/low", net::LOWEST));

  EXPECT_TRUE(high->started());
  EXPECT_FALSE(high2->started());
  EXPECT_FALSE(low->started());

  FireCoalescingTimer();

  EXPECT_TRUE(high->started());
  EXPECT_TRUE(high2->started());
  EXPECT_TRUE(low->started());
}

}  // unnamed namespace

}  // namespace content
