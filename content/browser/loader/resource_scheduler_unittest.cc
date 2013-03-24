// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_scheduler.h"

#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/common/resource_messages.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/process_type.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/resource_type.h"

namespace content {

namespace {

class TestRequestFactory;

const int kChildId = 30;
const int kRouteId = 75;

class TestRequest : public ResourceController {
 public:
  TestRequest(scoped_ptr<ResourceThrottle> throttle,
              scoped_ptr<net::URLRequest> url_request)
      : started_(false),
        throttle_(throttle.Pass()),
        url_request_(url_request.Pass()) {
    throttle_->set_controller_for_testing(this);
  }

  bool started() const { return started_; }

  void Start() {
    bool deferred = false;
    throttle_->WillStartRequest(&deferred);
    started_ = !deferred;
  }

  const net::URLRequest* url_request() const { return url_request_.get(); }

 protected:
  // ResourceController interface:
  virtual void Cancel() OVERRIDE {}
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
      : TestRequest(throttle.Pass(), url_request.Pass()) {
  }

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
};

class FakeURLRequestContextSelector
    : public ResourceMessageFilter::URLRequestContextSelector {
 private:
  virtual net::URLRequestContext* GetRequestContext(
      ResourceType::Type) OVERRIDE {
    return NULL;
  }
};

class FakeResourceMessageFilter : public ResourceMessageFilter {
 public:
  FakeResourceMessageFilter(int child_id)
      : ResourceMessageFilter(child_id,
                              PROCESS_TYPE_RENDERER,
                              &context_,
                              NULL  /* appcache_service */,
                              NULL  /* blob_storage_context */,
                              NULL  /* file_system_context */,
                              new FakeURLRequestContextSelector) {
  }

 private:
  virtual ~FakeResourceMessageFilter() {}

  FakeResourceContext context_;
};

class ResourceSchedulerTest : public testing::Test {
 protected:
  ResourceSchedulerTest()
      : next_request_id_(0),
        message_loop_(MessageLoop::TYPE_IO),
        ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {
    scheduler_.OnClientCreated(kChildId, kRouteId);
  }

  virtual ~ResourceSchedulerTest() {
    scheduler_.OnClientDeleted(kChildId, kRouteId);
  }

  scoped_ptr<net::URLRequest> NewURLRequestWithRoute(
      const char* url,
      net::RequestPriority priority,
      int route_id) {
    scoped_ptr<net::URLRequest> url_request(
        context_.CreateRequest(GURL(url), NULL));
    url_request->SetPriority(priority);
    ResourceRequestInfoImpl* info = new ResourceRequestInfoImpl(
        PROCESS_TYPE_RENDERER,             // process_type
        kChildId,                          // child_id
        route_id,                          // route_id
        0,                                 // origin_pid
        ++next_request_id_,                // request_id
        false,                             // is_main_frame
        0,                                 // frame_id
        false,                             // parent_is_main_frame
        0,                                 // parent_frame_id
        ResourceType::SUB_RESOURCE,        // resource_type
        PAGE_TRANSITION_LINK,              // transition_type
        false,                             // is_download
        false,                             // is_stream
        true,                              // allow_download
        false,                             // has_user_gesture
        WebKit::WebReferrerPolicyDefault,  // referrer_policy
        NULL,                              // context
        true);                             // is_async
    info->AssociateWithRequest(url_request.get());
    return url_request.Pass();
  }

  scoped_ptr<net::URLRequest> NewURLRequest(const char* url,
                                            net::RequestPriority priority) {
    return NewURLRequestWithRoute(url, priority, kRouteId);
  }

  TestRequest* NewRequestWithRoute(const char* url,
                                   net::RequestPriority priority,
                                   int route_id) {
    scoped_ptr<net::URLRequest> url_request(
        NewURLRequestWithRoute(url, priority, route_id));
    scoped_ptr<ResourceThrottle> throttle(scheduler_.ScheduleRequest(
        kChildId, route_id, url_request.get()));
    TestRequest* request = new TestRequest(throttle.Pass(), url_request.Pass());
    request->Start();
    return request;
  }

  TestRequest* NewRequest(const char* url, net::RequestPriority priority) {
    return NewRequestWithRoute(url, priority, kRouteId);
  }

  void ChangeRequestPriority(TestRequest* request,
                             net::RequestPriority new_priority) {
    scoped_refptr<FakeResourceMessageFilter> filter(
        new FakeResourceMessageFilter(kChildId));
    const ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(
        request->url_request());
    const GlobalRequestID& id = info->GetGlobalRequestID();
    ResourceHostMsg_DidChangePriority msg(
        kRouteId, id.request_id, new_priority);
    bool ok = false;
    rdh_.OnMessageReceived(msg, filter.get(), &ok);
    EXPECT_TRUE(ok);
  }

  int next_request_id_;
  MessageLoop message_loop_;
  BrowserThreadImpl ui_thread_;
  BrowserThreadImpl io_thread_;
  ResourceDispatcherHostImpl rdh_;
  ResourceScheduler scheduler_;
  net::TestURLRequestContext context_;
};

TEST_F(ResourceSchedulerTest, OneIsolatedLowRequest) {
  scoped_ptr<TestRequest> request(NewRequest("http://host/1", net::LOWEST));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, LowBlocksUntilIdle) {
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_FALSE(low->started());
  high.reset();
  EXPECT_TRUE(low->started());
}

TEST_F(ResourceSchedulerTest, LowBlocksUntilBodyInserted) {
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_FALSE(low->started());
  scheduler_.OnWillInsertBody(kChildId, kRouteId);
  EXPECT_TRUE(low->started());
}

TEST_F(ResourceSchedulerTest, NavigationResetsState) {
  scheduler_.OnWillInsertBody(kChildId, kRouteId);
  scheduler_.OnNavigate(kChildId, kRouteId);
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_FALSE(low->started());
}

TEST_F(ResourceSchedulerTest, BackgroundRequestStartsImmediately) {
  const int route_id = 0;  // Indicates a background request.
  scoped_ptr<TestRequest> request(NewRequestWithRoute("http://host/1",
                                                      net::LOWEST, route_id));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, StartRequestsWhenIdle) {
  scoped_ptr<TestRequest> high1(NewRequest("http://host/high1", net::HIGHEST));
  scoped_ptr<TestRequest> high2(NewRequest("http://host/high2", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high1->started());
  EXPECT_TRUE(high2->started());
  EXPECT_FALSE(low->started());
  high1.reset();
  EXPECT_FALSE(low->started());
  high2.reset();
  EXPECT_TRUE(low->started());
}

TEST_F(ResourceSchedulerTest, CancelOtherRequestsWhileResuming) {
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low1(NewRequest("http://host/low1", net::LOWEST));

  scoped_ptr<net::URLRequest> url_request(
      NewURLRequest("http://host/low2", net::LOWEST));
  scoped_ptr<ResourceThrottle> throttle(scheduler_.ScheduleRequest(
      kChildId, kRouteId, url_request.get()));
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
  ScopedVector<TestRequest> lows;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient; ++i) {
    string url = "http://host/low" + base::IntToString(i);
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows[i]->started());
  }

  scoped_ptr<TestRequest> last(NewRequest("http://host/last", net::LOWEST));
  EXPECT_FALSE(last->started());
  high.reset();
  EXPECT_FALSE(last->started());
  lows.erase(lows.begin());
  EXPECT_TRUE(last->started());
}

TEST_F(ResourceSchedulerTest, RaisePriorityAndStart) {
  // Dummy to enforce scheduling.
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));

  scoped_ptr<TestRequest> request(NewRequest("http://host/req", net::LOWEST));
  EXPECT_FALSE(request->started());

  ChangeRequestPriority(request.get(), net::HIGHEST);
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, RaisePriorityInQueue) {
  // Dummy to enforce scheduling.
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));

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
  EXPECT_TRUE(request->started());
  EXPECT_FALSE(idle->started());
}

TEST_F(ResourceSchedulerTest, LowerPriority) {
  // Dummy to enforce scheduling.
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));

  scoped_ptr<TestRequest> request(NewRequest("http://host/req", net::LOWEST));
  scoped_ptr<TestRequest> idle(NewRequest("http://host/idle", net::IDLE));
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  ChangeRequestPriority(request.get(), net::IDLE);
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.
  ScopedVector<TestRequest> lows;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient - 1; ++i) {
    string url = "http://host/low" + base::IntToString(i);
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }

  scheduler_.OnWillInsertBody(kChildId, kRouteId);
  EXPECT_FALSE(request->started());
  EXPECT_TRUE(idle->started());
}

TEST_F(ResourceSchedulerTest, ReprioritizedRequestGoesToBackOfQueue) {
  // Dummy to enforce scheduling.
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));

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

}  // unnamed namespace

}  // namespace content
