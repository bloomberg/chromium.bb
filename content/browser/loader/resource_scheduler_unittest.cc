// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_scheduler.h"

#include "base/message_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_throttle.h"
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

class ResourceSchedulerTest : public testing::Test {
 protected:
  ResourceSchedulerTest()
      : message_loop_(MessageLoop::TYPE_IO),
        ui_thread_(BrowserThread::UI, &message_loop_) {
    scheduler_.OnClientCreated(kChildId, kRouteId);
  }

  virtual ~ResourceSchedulerTest() {
    scheduler_.OnClientDeleted(kChildId, kRouteId);
  }

  scoped_ptr<net::URLRequest> NewURLRequest(const char* url,
                                            net::RequestPriority priority,
                                            int route_id = kRouteId) {
    scoped_ptr<net::URLRequest> url_request(
        context_.CreateRequest(GURL(url), NULL));
    url_request->set_priority(priority);
    ResourceRequestInfo::AllocateForTesting(
        url_request.get(), ResourceType::SUB_RESOURCE, NULL, kChildId,
        route_id);
    return url_request.Pass();
  }

  TestRequest* NewRequest(const char* url, net::RequestPriority priority,
                          int route_id = kRouteId) {
    scoped_ptr<net::URLRequest> url_request(
        NewURLRequest(url, priority, route_id));
    scoped_ptr<ResourceThrottle> throttle(scheduler_.ScheduleRequest(
        kChildId, route_id, url_request.get()));
    TestRequest* request = new TestRequest(throttle.Pass(), url_request.Pass());
    request->Start();
    return request;
  }

  MessageLoop message_loop_;
  BrowserThreadImpl ui_thread_;
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
  scoped_ptr<TestRequest> request(NewRequest("http://host/1", net::LOWEST,
                                             route_id));
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

}  // unnamed namespace

}  // namespace content
