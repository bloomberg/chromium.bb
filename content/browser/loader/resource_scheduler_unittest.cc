// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_scheduler.h"

#include <utility>

#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_entropy_provider.h"
#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host_factory.h"
#include "content/test/test_web_contents.h"
#include "net/base/host_port_pair.h"
#include "net/base/request_priority.h"
#include "net/http/http_server_properties_impl.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/latency_info.h"

using std::string;

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
  TestRequest(scoped_ptr<net::URLRequest> url_request,
              scoped_ptr<ResourceThrottle> throttle,
              ResourceScheduler* scheduler)
      : started_(false),
        url_request_(std::move(url_request)),
        throttle_(std::move(throttle)),
        scheduler_(scheduler) {
    throttle_->set_controller_for_testing(this);
  }
  ~TestRequest() override {
    // The URLRequest must still be valid when the ScheduledResourceRequest is
    // destroyed, so that it can unregister itself.
    throttle_.reset();
  }

  bool started() const { return started_; }

  void Start() {
    bool deferred = false;
    throttle_->WillStartRequest(&deferred);
    started_ = !deferred;
  }

  void ChangePriority(net::RequestPriority new_priority, int intra_priority) {
    scheduler_->ReprioritizeRequest(url_request_.get(), new_priority,
                                    intra_priority);
  }

  void Cancel() override {
    // Alert the scheduler that the request can be deleted.
    throttle_.reset();
  }

  const net::URLRequest* url_request() const { return url_request_.get(); }

 protected:
  // ResourceController interface:
  void CancelAndIgnore() override {}
  void CancelWithError(int error_code) override {}
  void Resume() override { started_ = true; }

 private:
  bool started_;
  scoped_ptr<net::URLRequest> url_request_;
  scoped_ptr<ResourceThrottle> throttle_;
  ResourceScheduler* scheduler_;
};

class CancelingTestRequest : public TestRequest {
 public:
  CancelingTestRequest(scoped_ptr<net::URLRequest> url_request,
                       scoped_ptr<ResourceThrottle> throttle,
                       ResourceScheduler* scheduler)
      : TestRequest(std::move(url_request), std::move(throttle), scheduler) {}

  void set_request_to_cancel(scoped_ptr<TestRequest> request_to_cancel) {
    request_to_cancel_ = std::move(request_to_cancel);
  }

 private:
  void Resume() override {
    TestRequest::Resume();
    request_to_cancel_.reset();
  }

  scoped_ptr<TestRequest> request_to_cancel_;
};

class FakeResourceContext : public ResourceContext {
 private:
  net::HostResolver* GetHostResolver() override { return NULL; }
  net::URLRequestContext* GetRequestContext() override { return NULL; }
};

class ResourceSchedulerTest : public testing::Test {
 protected:
  ResourceSchedulerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_),
        field_trial_list_(new base::MockEntropyProvider()) {
    InitializeScheduler();
    context_.set_http_server_properties(http_server_properties_.GetWeakPtr());
  }

  ~ResourceSchedulerTest() override {
    CleanupScheduler();
  }

  // Done separately from construction to allow for modification of command
  // line flags in tests.
  void InitializeScheduler() {
    CleanupScheduler();

    // Destroys previous scheduler, also destroys any previously created
    // mock_timer_.
    scheduler_.reset(new ResourceScheduler());

    mock_timer_ = new base::MockTimer(true, true);
    scheduler_->set_timer_for_testing(scoped_ptr<base::Timer>(mock_timer_));

    // TODO(aiolos): Remove when throttling and coalescing have both landed.
    scheduler_->SetThrottleOptionsForTesting(true /* should_throttle */,
                                             false /* should_coalesce */);

    scheduler_->OnClientCreated(kChildId, kRouteId, true, false);
    scheduler_->OnClientCreated(
        kBackgroundChildId, kBackgroundRouteId, false, false);
  }

  void CleanupScheduler() {
    if (scheduler_) {
      scheduler_->OnClientDeleted(kChildId, kRouteId);
      scheduler_->OnClientDeleted(kBackgroundChildId, kBackgroundRouteId);
    }
  }

  // Create field trials based on the argument, which has the same format
  // as the argument to kForceFieldTrials.
  bool InitializeFieldTrials(const std::string& force_field_trial_argument) {
    return base::FieldTrialList::CreateTrialsFromString(
        force_field_trial_argument,
        base::FieldTrialList::DONT_ACTIVATE_TRIALS,
        std::set<std::string>());
  }

  scoped_ptr<net::URLRequest> NewURLRequestWithChildAndRoute(
      const char* url,
      net::RequestPriority priority,
      int child_id,
      int route_id) {
    scoped_ptr<net::URLRequest> url_request(
        context_.CreateRequest(GURL(url), priority, NULL));
    return url_request;
  }

  scoped_ptr<net::URLRequest> NewURLRequest(const char* url,
                                            net::RequestPriority priority) {
    return NewURLRequestWithChildAndRoute(url, priority, kChildId, kRouteId);
  }

  TestRequest* NewRequestWithRoute(const char* url,
                                   net::RequestPriority priority,
                                   int route_id) {
    return NewRequestWithChildAndRoute(url, priority, kChildId, route_id);
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
    scoped_ptr<net::URLRequest> url_request(
        NewURLRequestWithChildAndRoute(url, priority, child_id, route_id));
    scoped_ptr<ResourceThrottle> throttle(scheduler_->ScheduleRequest(
        child_id, route_id, is_async, url_request.get()));
    TestRequest* request = new TestRequest(std::move(url_request),
                                           std::move(throttle), scheduler());
    request->Start();
    return request;
  }

  void ChangeRequestPriority(TestRequest* request,
                             net::RequestPriority new_priority,
                             int intra_priority = 0) {
    request->ChangePriority(new_priority, intra_priority);
  }

  void FireCoalescingTimer() {
    EXPECT_TRUE(mock_timer_->IsRunning());
    mock_timer_->Fire();
  }

  ResourceScheduler* scheduler() {
    return scheduler_.get();
  }

  base::MessageLoopForIO message_loop_;
  BrowserThreadImpl ui_thread_;
  BrowserThreadImpl io_thread_;
  ResourceDispatcherHostImpl rdh_;
  scoped_ptr<ResourceScheduler> scheduler_;
  base::FieldTrialList field_trial_list_;
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
  base::RunLoop().RunUntilIdle();
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
  base::RunLoop().RunUntilIdle();
  // TODO(mmenke):  The name of this test implies this should be false.
  // Investigate if this is now expected, remove or update this test if it is.
  EXPECT_TRUE(low2->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, OneLowLoadsUntilCriticalComplete) {
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  scoped_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(low2->started());

  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, LowDoesNotBlockCriticalComplete) {
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOW));
  scoped_ptr<TestRequest> lowest(NewRequest("http://host/lowest", net::LOWEST));
  scoped_ptr<TestRequest> lowest2(
      NewRequest("http://host/lowest", net::LOWEST));
  EXPECT_TRUE(low->started());
  EXPECT_TRUE(lowest->started());
  EXPECT_FALSE(lowest2->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(lowest2->started());
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

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, NavigationResetsState) {
  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  scheduler()->OnNavigate(kChildId, kRouteId);
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
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(low2->started());

  high2.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, CancelOtherRequestsWhileResuming) {
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low1(NewRequest("http://host/low1", net::LOWEST));

  scoped_ptr<net::URLRequest> url_request(
      NewURLRequest("http://host/low2", net::LOWEST));
  scoped_ptr<ResourceThrottle> throttle(scheduler()->ScheduleRequest(
      kChildId, kRouteId, true, url_request.get()));
  scoped_ptr<CancelingTestRequest> low2(new CancelingTestRequest(
      std::move(url_request), std::move(throttle), scheduler()));
  low2->Start();

  scoped_ptr<TestRequest> low3(NewRequest("http://host/low3", net::LOWEST));
  low2->set_request_to_cancel(std::move(low3));
  scoped_ptr<TestRequest> low4(NewRequest("http://host/low4", net::LOWEST));

  EXPECT_TRUE(high->started());
  EXPECT_FALSE(low2->started());

  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low1->started());
  EXPECT_TRUE(low2->started());
  EXPECT_TRUE(low4->started());
}

TEST_F(ResourceSchedulerTest, LimitedNumberOfDelayableRequestsInFlight) {
  // We only load low priority resources if there's a body.
  scheduler()->OnWillInsertBody(kChildId, kRouteId);

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
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(second_last_singlehost->started());
  EXPECT_FALSE(last_singlehost->started());

  lows_singlehost.erase(lows_singlehost.begin());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(last_singlehost->started());

  // Queue more requests from different hosts until we reach the total limit.
  int expected_slots_left =
      kMaxNumDelayableRequestsPerClient - kMaxNumDelayableRequestsPerHost;
  EXPECT_GT(expected_slots_left, 0);
  ScopedVector<TestRequest> lows_different_host;
  base::RunLoop().RunUntilIdle();
  for (int i = 0; i < expected_slots_left; ++i) {
    string url = "http://host" + base::IntToString(i) + "/low";
    lows_different_host.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows_different_host[i]->started());
  }

  scoped_ptr<TestRequest> last_different_host(NewRequest("http://host_new/last",
                                                         net::LOWEST));
  EXPECT_FALSE(last_different_host->started());
}

TEST_F(ResourceSchedulerTest, RaisePriorityAndStart) {
  // Dummies to enforce scheduling.
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/req", net::LOWEST));

  scoped_ptr<TestRequest> request(NewRequest("http://host/req", net::LOWEST));
  EXPECT_FALSE(request->started());

  ChangeRequestPriority(request.get(), net::HIGHEST);
  base::RunLoop().RunUntilIdle();
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
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.
  ScopedVector<TestRequest> lows;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient - 1; ++i) {
    string url = "http://host/low" + base::IntToString(i);
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  high.reset();
  base::RunLoop().RunUntilIdle();

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
  base::RunLoop().RunUntilIdle();
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

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  high.reset();
  base::RunLoop().RunUntilIdle();

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
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  ChangeRequestPriority(request.get(), net::LOWEST);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
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
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  high.reset();
  base::RunLoop().RunUntilIdle();
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
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  // Dummies to enforce scheduling.
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  scoped_ptr<TestRequest> request(
      NewSyncRequest("http://host/req", net::LOWEST));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, UnthrottledSyncSchedulesImmediately) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
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

  scheduler()->OnReceivedSpdyProxiedHttpResponse(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request->started());

  scoped_ptr<TestRequest> after(NewRequest("http://host/after", net::IDLE));
  EXPECT_TRUE(after->started());
}

TEST_F(ResourceSchedulerTest, NewSpdyHostInDelayableRequests) {
  scheduler()->OnWillInsertBody(kChildId, kRouteId);
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
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low1->started());

  low1.reset();
  base::RunLoop().RunUntilIdle();
  scoped_ptr<TestRequest> low2_spdy(
      NewRequest("http://spdyhost2:8080/low", net::IDLE));
  // Reprioritize a request after we learn the server supports SPDY.
  EXPECT_TRUE(low2_spdy->started());
  http_server_properties_.SetSupportsSpdy(
      net::HostPortPair("spdyhost2", 8080), true);
  ChangeRequestPriority(low2_spdy.get(), net::LOWEST);
  base::RunLoop().RunUntilIdle();
  scoped_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, ThrottledClientCreation) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  EXPECT_TRUE(scheduler()->should_throttle());
  scheduler()->OnClientCreated(
      kBackgroundChildId2, kBackgroundRouteId2, false, false);

  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  scheduler()->OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, ActiveClientThrottleUpdateOnLoadingChange) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, false);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
}

TEST_F(ResourceSchedulerTest, CoalesceBackgroundClientOnLoadCompletion) {
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
}

TEST_F(ResourceSchedulerTest, UnthrottleBackgroundClientOnLoadingStarted) {
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));

  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, false);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
}

TEST_F(ResourceSchedulerTest, OneRequestPerThrottledClient) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
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
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> request(
      NewBackgroundRequest("http://host/req", net::IDLE));
  EXPECT_FALSE(request->started());

  scheduler()->OnVisibilityChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                  kBackgroundRouteId));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, UnthrottleNewlyAudibleClient) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> request(
      NewBackgroundRequest("http://host/req", net::IDLE));
  EXPECT_FALSE(request->started());

  scheduler()->OnAudibilityChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, VisibleClientStillUnthrottledOnAudabilityChange) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));

  scheduler()->OnAudibilityChanged(kChildId, kRouteId, true);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));

  scheduler()->OnAudibilityChanged(kChildId, kRouteId, false);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
}

TEST_F(ResourceSchedulerTest, AudibleClientStillUnthrottledOnVisabilityChange) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler()->OnVisibilityChanged(kChildId, kRouteId, false);
  scheduler()->OnAudibilityChanged(kChildId, kRouteId, true);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));

  scheduler()->OnVisibilityChanged(kChildId, kRouteId, true);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));

  scheduler()->OnVisibilityChanged(kChildId, kRouteId, false);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
}

TEST_F(ResourceSchedulerTest, ThrottledClientStartsNextHighestPriorityRequest) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
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

  request->Cancel();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(high->started());
  EXPECT_FALSE(low->started());
}

TEST_F(ResourceSchedulerTest, ThrottledSpdyProxySchedulesImmediately) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> request(
      NewBackgroundRequest("http://host/req", net::IDLE));

  EXPECT_FALSE(request->started());

  scheduler()->OnReceivedSpdyProxiedHttpResponse(kBackgroundChildId,
                                                 kBackgroundRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request->started());

  scoped_ptr<TestRequest> after(
      NewBackgroundRequest("http://host/after", net::IDLE));
  EXPECT_TRUE(after->started());
}

TEST_F(ResourceSchedulerTest, CoalescedClientIssuesNoRequests) {
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> request(
      NewBackgroundRequest("http://host/req", net::IDLE));

  EXPECT_FALSE(high->started());
  EXPECT_FALSE(request->started());

  scheduler()->OnReceivedSpdyProxiedHttpResponse(kBackgroundChildId,
                                                 kBackgroundRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(high->started());

  scoped_ptr<TestRequest> after(
      NewBackgroundRequest("http://host/after", net::HIGHEST));
  EXPECT_FALSE(after->started());
}

TEST_F(ResourceSchedulerTest, CoalescedSpdyProxyWaits) {
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> request(
      NewBackgroundRequest("http://host/req", net::IDLE));

  EXPECT_FALSE(request->started());

  scheduler()->OnReceivedSpdyProxiedHttpResponse(kBackgroundChildId,
                                                 kBackgroundRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());

  scoped_ptr<TestRequest> after(
      NewBackgroundRequest("http://host/after", net::IDLE));
  EXPECT_FALSE(after->started());
}

TEST_F(ResourceSchedulerTest, ThrottledNonHTTPSchedulesImmediately) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
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
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
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
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
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
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
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
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_FALSE(scheduler()->active_clients_loaded());

  scheduler()->OnVisibilityChanged(kChildId, kRouteId, false);
  EXPECT_TRUE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));

  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));

  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, false);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));

  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
}

TEST_F(ResourceSchedulerTest,
       UnloadedClientVisibilityChangedCorrectlyUnthrottles) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler()->OnClientCreated(kChildId2, kRouteId2, false, false);
  scheduler()->OnClientCreated(
      kBackgroundChildId2, kBackgroundRouteId2, false, false);
  scheduler()->OnLoadingStateChanged(kChildId2, kRouteId2, true);
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);

  // 1 visible, 3 hidden
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 2 visible, 2 hidden
  scheduler()->OnVisibilityChanged(kChildId2, kRouteId2, true);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible, 3 hidden
  scheduler()->OnVisibilityChanged(kChildId2, kRouteId2, false);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  scheduler()->OnClientDeleted(kChildId2, kRouteId2);
  scheduler()->OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest,
       UnloadedClientAudibilityChangedCorrectlyUnthrottles) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler()->OnClientCreated(kChildId2, kRouteId2, false, false);
  scheduler()->OnClientCreated(
      kBackgroundChildId2, kBackgroundRouteId2, false, false);
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  scheduler()->OnVisibilityChanged(kChildId, kRouteId, false);
  scheduler()->OnAudibilityChanged(kChildId, kRouteId, true);

  // 1 audible, 3 hidden
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 2 audible, 2 hidden
  scheduler()->OnAudibilityChanged(kChildId2, kRouteId2, true);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 1 audible, 3 hidden
  scheduler()->OnAudibilityChanged(kChildId2, kRouteId2, false);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  scheduler()->OnClientDeleted(kChildId2, kRouteId2);
  scheduler()->OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest,
       LoadedClientVisibilityChangedCorrectlyUnthrottles) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler()->OnClientCreated(kChildId2, kRouteId2, false, false);
  scheduler()->OnClientCreated(
      kBackgroundChildId2, kBackgroundRouteId2, false, false);
  scheduler()->OnLoadingStateChanged(kChildId2, kRouteId2, true);
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  // 1 visible, 3 hidden
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 2 visible, 2 hidden
  scheduler()->OnVisibilityChanged(kChildId2, kRouteId2, true);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible, 3 hidden
  scheduler()->OnVisibilityChanged(kChildId2, kRouteId2, false);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  scheduler()->OnClientDeleted(kChildId2, kRouteId2);
  scheduler()->OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest,
       LoadedClientAudibilityChangedCorrectlyUnthrottles) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler()->OnClientCreated(kChildId2, kRouteId2, false, false);
  scheduler()->OnClientCreated(
      kBackgroundChildId2, kBackgroundRouteId2, false, false);
  scheduler()->OnLoadingStateChanged(kChildId2, kRouteId2, true);
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  scheduler()->OnVisibilityChanged(kChildId, kRouteId, false);
  scheduler()->OnAudibilityChanged(kChildId, kRouteId, true);
  // 1 audible, 3 hidden
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 2 audible, 2 hidden
  scheduler()->OnAudibilityChanged(kChildId2, kRouteId2, true);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 1 audible, 3 hidden
  scheduler()->OnAudibilityChanged(kChildId2, kRouteId2, false);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  scheduler()->OnClientDeleted(kChildId2, kRouteId2);
  scheduler()->OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, UnloadedClientBecomesHiddenCorrectlyUnthrottles) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler()->OnClientCreated(kChildId2, kRouteId2, true, false);
  scheduler()->OnClientCreated(
      kBackgroundChildId2, kBackgroundRouteId2, false, false);
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);

  // 2 visible, 2 hidden
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible, 3 hidden
  scheduler()->OnVisibilityChanged(kChildId2, kRouteId2, false);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 0 visible, 4 hidden
  scheduler()->OnVisibilityChanged(kChildId, kRouteId, false);
  EXPECT_TRUE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible, 3 hidden
  scheduler()->OnVisibilityChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  scheduler()->OnClientDeleted(kChildId2, kRouteId2);
  scheduler()->OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, UnloadedClientBecomesSilentCorrectlyUnthrottles) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler()->OnClientCreated(kChildId2, kRouteId2, false, true);
  scheduler()->OnClientCreated(
      kBackgroundChildId2, kBackgroundRouteId2, false, false);
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  scheduler()->OnAudibilityChanged(kChildId, kRouteId, true);
  scheduler()->OnVisibilityChanged(kChildId, kRouteId, false);
  // 2 audible, 2 hidden
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 1 audible, 3 hidden
  scheduler()->OnAudibilityChanged(kChildId2, kRouteId2, false);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 0 audible, 4 hidden
  scheduler()->OnAudibilityChanged(kChildId, kRouteId, false);
  EXPECT_TRUE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 1 audible, 3 hidden
  scheduler()->OnAudibilityChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  scheduler()->OnClientDeleted(kChildId2, kRouteId2);
  scheduler()->OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, LoadedClientBecomesHiddenCorrectlyThrottles) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler()->OnClientCreated(kChildId2, kRouteId2, true, false);
  scheduler()->OnClientCreated(
      kBackgroundChildId2, kBackgroundRouteId2, false, false);
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  scheduler()->OnLoadingStateChanged(kChildId2, kRouteId2, true);
  // 2 visible, 2 hidden
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible, 3 hidden
  scheduler()->OnVisibilityChanged(kChildId2, kRouteId2, false);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 0 visible, 4 hidden
  scheduler()->OnVisibilityChanged(kChildId, kRouteId, false);
  EXPECT_TRUE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible, 3 hidden
  scheduler()->OnVisibilityChanged(kChildId2, kRouteId2, true);
  EXPECT_TRUE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  scheduler()->OnClientDeleted(kChildId2, kRouteId2);
  scheduler()->OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, LoadedClientBecomesSilentCorrectlyThrottles) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler()->OnClientCreated(kChildId2, kRouteId2, false, true);
  scheduler()->OnClientCreated(
      kBackgroundChildId2, kBackgroundRouteId2, false, false);
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  scheduler()->OnLoadingStateChanged(kChildId2, kRouteId2, true);
  scheduler()->OnVisibilityChanged(kChildId, kRouteId, false);
  scheduler()->OnAudibilityChanged(kChildId, kRouteId, true);
  // 2 audible, 2 hidden
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 1 audible, 3 hidden
  scheduler()->OnAudibilityChanged(kChildId2, kRouteId2, false);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 0 audible, 4 hidden
  scheduler()->OnAudibilityChanged(kChildId, kRouteId, false);
  EXPECT_TRUE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 1 audible, 3 hidden
  scheduler()->OnAudibilityChanged(kChildId2, kRouteId2, true);
  EXPECT_TRUE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  scheduler()->OnClientDeleted(kChildId2, kRouteId2);
  scheduler()->OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, HiddenLoadedChangesCorrectlyStayThrottled) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler()->OnClientCreated(kChildId2, kRouteId2, true, false);
  scheduler()->OnClientCreated(
      kBackgroundChildId2, kBackgroundRouteId2, false, false);

  // 1 visible and 2 hidden loading, 1 visible loaded
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible and 1 hidden loading, 1 visible and 1 hidden loaded
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible loading, 1 visible and 2 hidden loaded
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible and 1 hidden loading, 1 visible and 1 hidden loaded
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  scheduler()->OnClientDeleted(kChildId2, kRouteId2);
  scheduler()->OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, PartialVisibleClientLoadedDoesNotUnthrottle) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler()->OnClientCreated(kChildId2, kRouteId2, true, false);
  scheduler()->OnClientCreated(
      kBackgroundChildId2, kBackgroundRouteId2, false, false);

  // 2 visible loading, 1 hidden loading, 1 hidden loaded
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible and 1 hidden loaded, 1 visible and 1 hidden loading
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 2 visible loading, 1 hidden loading, 1 hidden loaded
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, false);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  scheduler()->OnClientDeleted(kChildId2, kRouteId2);
  scheduler()->OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, FullVisibleLoadedCorrectlyUnthrottle) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler()->OnClientCreated(kChildId2, kRouteId2, true, false);
  scheduler()->OnClientCreated(
      kBackgroundChildId2, kBackgroundRouteId2, false, false);

  // 1 visible and 1 hidden loaded, 1 visible and 1 hidden loading
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  scheduler()->OnLoadingStateChanged(kChildId2, kRouteId2, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(
      NewBackgroundRequest("http://host/low", net::LOWEST));

  EXPECT_TRUE(high->started());
  EXPECT_FALSE(low->started());

  // 2 visible loaded, 1 hidden loading, 1 hidden loaded
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  // kBackgroundClientId unthrottling should unthrottle it's request.
  EXPECT_TRUE(low->started());

  // 1 visible and 1 hidden loaded, 1 visible and 1 hidden loading
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  scheduler()->OnClientDeleted(kChildId2, kRouteId2);
  scheduler()->OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest,
       ActiveAndLoadingClientDeletedCorrectlyUnthrottle) {
  // TODO(aiolos): remove when throttling and coalescing have both landed
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          false /* should_coalesce */);
  scheduler()->OnClientCreated(kChildId2, kRouteId2, true, false);
  scheduler()->OnClientCreated(
      kBackgroundChildId2, kBackgroundRouteId2, false, false);

  // 1 visible and 1 hidden loaded, 1 visible and 1 hidden loading
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId2, kRouteId2));

  // 1 visible loaded, 1 hidden loading, 1 hidden loaded
  scheduler()->OnClientDeleted(kChildId2, kRouteId2);
  EXPECT_TRUE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  // 1 visible and 1 hidden loaded, 1 visible and 1 hidden loading
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, false);
  EXPECT_FALSE(scheduler()->active_clients_loaded());
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));

  scheduler()->OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, CoalescedClientCreationStartsTimer) {
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(mock_timer_->IsRunning());
}

TEST_F(ResourceSchedulerTest, ActiveLoadingClientLoadedAndHiddenStartsTimer) {
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  EXPECT_FALSE(mock_timer_->IsRunning());
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_FALSE(mock_timer_->IsRunning());

  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_FALSE(mock_timer_->IsRunning());

  scheduler()->OnVisibilityChanged(kChildId, kRouteId, false);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(mock_timer_->IsRunning());
}

TEST_F(ResourceSchedulerTest, ActiveLoadingClientHiddenAndLoadedStartsTimer) {
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::THROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_FALSE(mock_timer_->IsRunning());

  scheduler()->OnVisibilityChanged(kChildId, kRouteId, false);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_FALSE(mock_timer_->IsRunning());

  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kChildId, kRouteId));
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(mock_timer_->IsRunning());
}

TEST_F(ResourceSchedulerTest, CoalescedClientBecomesAudibleStopsTimer) {
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(mock_timer_->IsRunning());

  scheduler()->OnAudibilityChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_FALSE(mock_timer_->IsRunning());
}

TEST_F(ResourceSchedulerTest, LastCoalescedClientDeletionStopsTimer) {
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler()->OnClientCreated(
      kBackgroundChildId2, kBackgroundRouteId2, false, false);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_TRUE(mock_timer_->IsRunning());

  scheduler()->OnClientDeleted(kBackgroundChildId, kBackgroundRouteId);
  EXPECT_TRUE(mock_timer_->IsRunning());

  scheduler()->OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
  EXPECT_FALSE(mock_timer_->IsRunning());

  // To avoid errors on test tear down.
  scheduler()->OnClientCreated(
      kBackgroundChildId, kBackgroundRouteId, false, false);
}

TEST_F(ResourceSchedulerTest, LastCoalescedClientStartsLoadingStopsTimer) {
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler()->OnClientCreated(
      kBackgroundChildId2, kBackgroundRouteId2, false, false);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_TRUE(mock_timer_->IsRunning());

  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, false);
  EXPECT_TRUE(mock_timer_->IsRunning());

  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, false);
  EXPECT_FALSE(mock_timer_->IsRunning());

  // This is needed to avoid errors on test tear down.
  scheduler()->OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest, LastCoalescedClientBecomesVisibleStopsTimer) {
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler()->OnClientCreated(
      kBackgroundChildId2, kBackgroundRouteId2, false, false);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId2,
                                                kBackgroundRouteId2));
  EXPECT_TRUE(mock_timer_->IsRunning());

  scheduler()->OnVisibilityChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_TRUE(mock_timer_->IsRunning());

  scheduler()->OnVisibilityChanged(
      kBackgroundChildId2, kBackgroundRouteId2, true);
  EXPECT_FALSE(mock_timer_->IsRunning());

  // To avoid errors on test tear down.
  scheduler()->OnClientDeleted(kBackgroundChildId2, kBackgroundRouteId2);
}

TEST_F(ResourceSchedulerTest,
       CoalescedClientBecomesLoadingAndVisibleStopsTimer) {
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  EXPECT_FALSE(mock_timer_->IsRunning());
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(mock_timer_->IsRunning());

  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, false);
  EXPECT_EQ(ResourceScheduler::UNTHROTTLED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_FALSE(mock_timer_->IsRunning());

  scheduler()->OnVisibilityChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::ACTIVE_AND_LOADING,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_FALSE(mock_timer_->IsRunning());
}

TEST_F(ResourceSchedulerTest, CoalescedRequestsIssueOnTimer) {
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(scheduler()->active_clients_loaded());

  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(
      NewBackgroundRequest("http://host/low", net::LOWEST));
  EXPECT_FALSE(high->started());
  EXPECT_FALSE(low->started());

  FireCoalescingTimer();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low->started());
}

TEST_F(ResourceSchedulerTest, CoalescedRequestsUnthrottleCorrectlyOnTimer) {
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);
  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(scheduler()->active_clients_loaded());

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
  base::RunLoop().RunUntilIdle();

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
  scheduler()->SetThrottleOptionsForTesting(true /* should_throttle */,
                                          true /* should_coalesce */);
  scheduler()->OnLoadingStateChanged(kChildId, kRouteId, true);
  scheduler()->OnLoadingStateChanged(
      kBackgroundChildId, kBackgroundRouteId, true);

  EXPECT_EQ(ResourceScheduler::COALESCED,
            scheduler()->GetClientStateForTesting(kBackgroundChildId,
                                                kBackgroundRouteId));
  EXPECT_TRUE(scheduler()->active_clients_loaded());

  scoped_ptr<TestRequest> high(
      NewBackgroundRequest("http://host/high", net::HIGHEST));
  EXPECT_FALSE(high->started());

  FireCoalescingTimer();
  base::RunLoop().RunUntilIdle();

  scoped_ptr<TestRequest> high2(
      NewBackgroundRequest("http://host/high2", net::HIGHEST));
  scoped_ptr<TestRequest> low(
      NewBackgroundRequest("http://host/low", net::LOWEST));

  EXPECT_TRUE(high->started());
  EXPECT_FALSE(high2->started());
  EXPECT_FALSE(low->started());

  FireCoalescingTimer();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(high->started());
  EXPECT_TRUE(high2->started());
  EXPECT_TRUE(low->started());
}

TEST_F(ResourceSchedulerTest, GetVisualSignalFromRenderViewHost) {
  scoped_ptr<MockRenderProcessHostFactory> render_process_host_factory;
  scoped_ptr<TestRenderViewHostFactory> render_view_host_factory;
  scoped_ptr<TestBrowserContext> browser_context;
  scoped_ptr<TestWebContents> web_contents_1;
  scoped_ptr<TestWebContents> web_contents_2;
  render_process_host_factory.reset(new MockRenderProcessHostFactory());
  render_view_host_factory.reset(
      new TestRenderViewHostFactory(render_process_host_factory.get()));

  browser_context.reset(new TestBrowserContext());
  scoped_refptr<SiteInstance> site_instance_1 =
      SiteInstance::Create(browser_context.get());
  scoped_refptr<SiteInstance> site_instance_2 =
      SiteInstance::Create(browser_context.get());
  SiteInstanceImpl::set_render_process_host_factory(
          render_process_host_factory.get());

  web_contents_1.reset(
      TestWebContents::Create(browser_context.get(), site_instance_1.get()));
  web_contents_2.reset(
      TestWebContents::Create(browser_context.get(), site_instance_2.get()));
  base::RunLoop().RunUntilIdle();

  RenderViewHostImpl* rvh1 = web_contents_1->GetRenderViewHost();
  RenderViewHostImpl* rvh2 = web_contents_2->GetRenderViewHost();
  ResourceScheduler* scheduler = ResourceDispatcherHostImpl::Get()->scheduler();

  // Check initial visibility is set correctly.
  EXPECT_EQ(scheduler->IsClientVisibleForTesting(rvh1->GetProcess()->GetID(),
                                                 rvh1->GetRoutingID()),
            !rvh1->GetWidget()->is_hidden());
  EXPECT_EQ(scheduler->IsClientVisibleForTesting(rvh2->GetProcess()->GetID(),
                                                 rvh1->GetRoutingID()),
            !rvh2->GetWidget()->is_hidden());

  // 1 visible, 1 hidden.
  rvh1->GetWidget()->WasShown(ui::LatencyInfo());
  rvh2->GetWidget()->WasHidden();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(scheduler->IsClientVisibleForTesting(rvh1->GetProcess()->GetID(),
                                                   rvh1->GetRoutingID()));
  EXPECT_FALSE(scheduler->IsClientVisibleForTesting(rvh2->GetProcess()->GetID(),
                                                    rvh2->GetRoutingID()));

  // Flip the visibility and check again.
  rvh1->GetWidget()->WasHidden();
  rvh2->GetWidget()->WasShown(ui::LatencyInfo());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(scheduler->IsClientVisibleForTesting(rvh1->GetProcess()->GetID(),
                                                    rvh1->GetRoutingID()));
  EXPECT_TRUE(scheduler->IsClientVisibleForTesting(rvh2->GetProcess()->GetID(),
                                                   rvh2->GetRoutingID()));
  // Clean up.
  web_contents_1.reset();
  web_contents_2.reset();
  base::RunLoop().RunUntilIdle();

  browser_context.reset();
  render_process_host_factory.reset();
}

TEST_F(ResourceSchedulerTest, OustandingRequestLimitEnforced) {
  const int kRequestLimit = 3;
  ASSERT_TRUE(InitializeFieldTrials(
      base::StringPrintf("OutstandingRequestLimiting/Limit=%d/",
                         kRequestLimit)));
  InitializeScheduler();

  // Throw in requests up to the above limit; make sure they are started.
  ScopedVector<TestRequest> requests;
  for (int i = 0; i < kRequestLimit; ++i) {
    string url = "http://host/medium";
    requests.push_back(NewRequest(url.c_str(), net::MEDIUM));
    EXPECT_TRUE(requests[i]->started());
  }

  // Confirm that another request will indeed fail.
  string url = "http://host/medium";
  requests.push_back(NewRequest(url.c_str(), net::MEDIUM));
  EXPECT_FALSE(requests[kRequestLimit]->started());
}

// Confirm that outstanding requests limits apply to requests to hosts
// that support request priority.
TEST_F(ResourceSchedulerTest,
       OutstandingRequestsLimitsEnforcedForRequestPriority) {
  const int kRequestLimit = 3;
  ASSERT_TRUE(InitializeFieldTrials(
      base::StringPrintf("OutstandingRequestLimiting/Limit=%d/",
                         kRequestLimit)));
  InitializeScheduler();

  http_server_properties_.SetSupportsSpdy(
      net::HostPortPair("spdyhost", 443), true);

  // Throw in requests up to the above limit; make sure they are started.
  ScopedVector<TestRequest> requests;
  for (int i = 0; i < kRequestLimit; ++i) {
    string url = "http://spdyhost/medium";
    requests.push_back(NewRequest(url.c_str(), net::MEDIUM));
    EXPECT_TRUE(requests[i]->started());
  }

  // Confirm that another request will indeed fail.
  string url = "http://spdyhost/medium";
  requests.push_back(NewRequest(url.c_str(), net::MEDIUM));
  EXPECT_FALSE(requests[kRequestLimit]->started());
}

TEST_F(ResourceSchedulerTest, OutstandingRequestLimitDelays) {
  const int kRequestLimit = 3;
  ASSERT_TRUE(InitializeFieldTrials(
      base::StringPrintf("OutstandingRequestLimiting/Limit=%d/",
                         kRequestLimit)));

  InitializeScheduler();
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  scoped_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_FALSE(low->started());
  EXPECT_FALSE(low2->started());

  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low->started());
  EXPECT_TRUE(low2->started());
}

// Async revalidations which are not started when the tab is closed must be
// started at some point, or they will hang around forever and prevent other
// async revalidations to the same URL from being issued.
TEST_F(ResourceSchedulerTest, RequestStartedAfterClientDeleted) {
  scheduler_->OnClientCreated(kChildId2, kRouteId2, false, false);
  scoped_ptr<TestRequest> high(NewRequestWithChildAndRoute(
      "http://host/high", net::HIGHEST, kChildId2, kRouteId2));
  scoped_ptr<TestRequest> lowest1(NewRequestWithChildAndRoute(
      "http://host/lowest", net::LOWEST, kChildId2, kRouteId2));
  scoped_ptr<TestRequest> lowest2(NewRequestWithChildAndRoute(
      "http://host/lowest", net::LOWEST, kChildId2, kRouteId2));
  EXPECT_FALSE(lowest2->started());

  scheduler_->OnClientDeleted(kChildId2, kRouteId2);
  high.reset();
  lowest1.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(lowest2->started());
}

// The ResourceScheduler::Client destructor calls
// LoadAnyStartablePendingRequests(), which may start some pending requests.
// This test is to verify that requests will be started at some point
// even if they were not started by the destructor.
TEST_F(ResourceSchedulerTest, RequestStartedAfterClientDeletedManyDelayable) {
  scheduler_->OnClientCreated(kChildId2, kRouteId2, false, false);
  scoped_ptr<TestRequest> high(NewRequestWithChildAndRoute(
      "http://host/high", net::HIGHEST, kChildId2, kRouteId2));
  const int kMaxNumDelayableRequestsPerClient = 10;
  ScopedVector<TestRequest> delayable_requests;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient + 1; ++i) {
    delayable_requests.push_back(NewRequestWithChildAndRoute(
        "http://host/lowest", net::LOWEST, kChildId2, kRouteId2));
  }
  scoped_ptr<TestRequest> lowest(NewRequestWithChildAndRoute(
      "http://host/lowest", net::LOWEST, kChildId2, kRouteId2));
  EXPECT_FALSE(lowest->started());

  scheduler_->OnClientDeleted(kChildId2, kRouteId2);
  high.reset();
  delayable_requests.clear();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(lowest->started());
}

TEST_F(ResourceSchedulerTest, DefaultLayoutBlockingPriority) {
  const int kDeferLateScripts = 0;
  const int kIncreaseFontPriority = 0;
  const int kIncreaseAsyncScriptPriority = 0;
  const int kEnablePriorityIncrease = 0;
  const int kEnableLayoutBlockingThreshold = 0;
  const int kLayoutBlockingThreshold = 0;
  const int kMaxNumDelayableWhileLayoutBlocking = 1;
  const int kMaxNumDelayableRequestsPerClient = 10;
  ASSERT_TRUE(InitializeFieldTrials(base::StringPrintf(
      "ResourcePriorities/LayoutBlocking_%d%d%d%d%d_%d_%d_%d/",
      kDeferLateScripts,
      kIncreaseFontPriority,
      kIncreaseAsyncScriptPriority,
      kEnablePriorityIncrease,
      kEnableLayoutBlockingThreshold,
      kLayoutBlockingThreshold,
      kMaxNumDelayableWhileLayoutBlocking,
      kMaxNumDelayableRequestsPerClient)));
  InitializeScheduler();
  scoped_ptr<TestRequest> high(
      NewRequest("http://hosthigh/high", net::HIGHEST));
  scoped_ptr<TestRequest> high2(
      NewRequest("http://hosthigh/high", net::HIGHEST));
  scoped_ptr<TestRequest> medium(
      NewRequest("http://hostmedium/medium", net::MEDIUM));
  scoped_ptr<TestRequest> medium2(
      NewRequest("http://hostmedium/medium", net::MEDIUM));
  scoped_ptr<TestRequest> low(NewRequest("http://hostlow/low", net::LOW));
  scoped_ptr<TestRequest> low2(NewRequest("http://hostlow/low", net::LOW));
  scoped_ptr<TestRequest> lowest(NewRequest("http://hostlowest/lowest", net::LOWEST));
  scoped_ptr<TestRequest> lowest2(
      NewRequest("http://hostlowest/lowest", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(high2->started());
  EXPECT_TRUE(medium->started());
  EXPECT_TRUE(medium2->started());
  EXPECT_TRUE(low->started());
  EXPECT_TRUE(low2->started());
  EXPECT_TRUE(lowest->started());
  EXPECT_FALSE(lowest2->started());

  lowest.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(lowest2->started());
}

TEST_F(ResourceSchedulerTest, IncreaseLayoutBlockingPriority) {
  // Changes the level of priorities that are allowed during layout-blocking
  // from net::LOWEST to net::LOW.
  const int kDeferLateScripts = 0;
  const int kIncreaseFontPriority = 0;
  const int kIncreaseAsyncScriptPriority = 0;
  const int kEnablePriorityIncrease = 1;
  const int kEnableLayoutBlockingThreshold = 0;
  const int kLayoutBlockingThreshold = 0;
  const int kMaxNumDelayableWhileLayoutBlocking = 1;
  const int kMaxNumDelayableRequestsPerClient = 10;
  ASSERT_TRUE(InitializeFieldTrials(base::StringPrintf(
      "ResourcePriorities/LayoutBlocking_%d%d%d%d%d_%d_%d_%d/",
      kDeferLateScripts,
      kIncreaseFontPriority,
      kIncreaseAsyncScriptPriority,
      kEnablePriorityIncrease,
      kEnableLayoutBlockingThreshold,
      kLayoutBlockingThreshold,
      kMaxNumDelayableWhileLayoutBlocking,
      kMaxNumDelayableRequestsPerClient)));
  InitializeScheduler();
  scoped_ptr<TestRequest> high(
      NewRequest("http://hosthigh/high", net::HIGHEST));
  scoped_ptr<TestRequest> high2(
      NewRequest("http://hosthigh/high", net::HIGHEST));
  scoped_ptr<TestRequest> medium(
      NewRequest("http://hostmedium/medium", net::MEDIUM));
  scoped_ptr<TestRequest> medium2(
      NewRequest("http://hostmedium/medium", net::MEDIUM));
  scoped_ptr<TestRequest> low(NewRequest("http://hostlow/low", net::LOW));
  scoped_ptr<TestRequest> low2(NewRequest("http://hostlow/low", net::LOW));
  scoped_ptr<TestRequest> lowest(NewRequest("http://hostlowest/lowest", net::LOWEST));
  scoped_ptr<TestRequest> lowest2(
      NewRequest("http://hostlowest/lowest", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(high2->started());
  EXPECT_TRUE(medium->started());
  EXPECT_TRUE(medium2->started());
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());
  EXPECT_FALSE(lowest->started());
  EXPECT_FALSE(lowest2->started());

  low.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low2->started());
  EXPECT_FALSE(lowest->started());
  EXPECT_FALSE(lowest2->started());

  low2.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(lowest->started());
  EXPECT_FALSE(lowest2->started());

  lowest.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(lowest2->started());
}

TEST_F(ResourceSchedulerTest, UseLayoutBlockingThresholdOne) {
  // Prevents any low priority requests from starting while more than
  // N high priority requests are pending (before body).
  const int kDeferLateScripts = 0;
  const int kIncreaseFontPriority = 0;
  const int kIncreaseAsyncScriptPriority = 0;
  const int kEnablePriorityIncrease = 0;
  const int kEnableLayoutBlockingThreshold = 1;
  const int kLayoutBlockingThreshold = 1;
  const int kMaxNumDelayableWhileLayoutBlocking = 1;
  const int kMaxNumDelayableRequestsPerClient = 10;
  ASSERT_TRUE(InitializeFieldTrials(base::StringPrintf(
      "ResourcePriorities/LayoutBlocking_%d%d%d%d%d_%d_%d_%d/",
      kDeferLateScripts,
      kIncreaseFontPriority,
      kIncreaseAsyncScriptPriority,
      kEnablePriorityIncrease,
      kEnableLayoutBlockingThreshold,
      kLayoutBlockingThreshold,
      kMaxNumDelayableWhileLayoutBlocking,
      kMaxNumDelayableRequestsPerClient)));
  InitializeScheduler();
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> high2(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  scoped_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(high2->started());
  EXPECT_FALSE(low->started());
  EXPECT_FALSE(low2->started());

  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());

  high2.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(low2->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, UseLayoutBlockingThresholdTwo) {
  // Prevents any low priority requests from starting while more than
  // N high priority requests are pending (before body).
  const int kDeferLateScripts = 0;
  const int kIncreaseFontPriority = 0;
  const int kIncreaseAsyncScriptPriority = 0;
  const int kEnablePriorityIncrease = 0;
  const int kEnableLayoutBlockingThreshold = 1;
  const int kLayoutBlockingThreshold = 2;
  const int kMaxNumDelayableWhileLayoutBlocking = 1;
  const int kMaxNumDelayableRequestsPerClient = 10;
  ASSERT_TRUE(InitializeFieldTrials(base::StringPrintf(
      "ResourcePriorities/LayoutBlocking_%d%d%d%d%d_%d_%d_%d/",
      kDeferLateScripts,
      kIncreaseFontPriority,
      kIncreaseAsyncScriptPriority,
      kEnablePriorityIncrease,
      kEnableLayoutBlockingThreshold,
      kLayoutBlockingThreshold,
      kMaxNumDelayableWhileLayoutBlocking,
      kMaxNumDelayableRequestsPerClient)));
  InitializeScheduler();
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> high2(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> high3(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  scoped_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(high2->started());
  EXPECT_TRUE(high3->started());
  EXPECT_FALSE(low->started());
  EXPECT_FALSE(low2->started());

  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());

  high2.reset();
  high3.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(low2->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, TwoDelayableLoadsUntilBodyInserted) {
  // Allow for two low priority requests to be in flight at any point in time
  // during the layout-blocking phase of loading.
  const int kDeferLateScripts = 0;
  const int kIncreaseFontPriority = 0;
  const int kIncreaseAsyncScriptPriority = 0;
  const int kEnablePriorityIncrease = 0;
  const int kEnableLayoutBlockingThreshold = 0;
  const int kLayoutBlockingThreshold = 0;
  const int kMaxNumDelayableWhileLayoutBlocking = 2;
  const int kMaxNumDelayableRequestsPerClient = 10;
  ASSERT_TRUE(InitializeFieldTrials(base::StringPrintf(
      "ResourcePriorities/LayoutBlocking_%d%d%d%d%d_%d_%d_%d/",
      kDeferLateScripts,
      kIncreaseFontPriority,
      kIncreaseAsyncScriptPriority,
      kEnablePriorityIncrease,
      kEnableLayoutBlockingThreshold,
      kLayoutBlockingThreshold,
      kMaxNumDelayableWhileLayoutBlocking,
      kMaxNumDelayableRequestsPerClient)));
  InitializeScheduler();
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  scoped_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  scoped_ptr<TestRequest> low3(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low->started());
  EXPECT_TRUE(low2->started());
  EXPECT_FALSE(low3->started());

  high.reset();
  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low3->started());
}

TEST_F(ResourceSchedulerTest,
    UseLayoutBlockingThresholdOneAndTwoDelayableLoadsUntilBodyInserted) {
  // Allow for two low priority requests to be in flight during the
  // layout-blocking phase of loading but only when there is not more than one
  // in-flight high priority request.
  const int kDeferLateScripts = 0;
  const int kIncreaseFontPriority = 0;
  const int kIncreaseAsyncScriptPriority = 0;
  const int kEnablePriorityIncrease = 0;
  const int kEnableLayoutBlockingThreshold = 1;
  const int kLayoutBlockingThreshold = 1;
  const int kMaxNumDelayableWhileLayoutBlocking = 2;
  const int kMaxNumDelayableRequestsPerClient = 10;
  ASSERT_TRUE(InitializeFieldTrials(base::StringPrintf(
      "ResourcePriorities/LayoutBlocking_%d%d%d%d%d_%d_%d_%d/",
      kDeferLateScripts,
      kIncreaseFontPriority,
      kIncreaseAsyncScriptPriority,
      kEnablePriorityIncrease,
      kEnableLayoutBlockingThreshold,
      kLayoutBlockingThreshold,
      kMaxNumDelayableWhileLayoutBlocking,
      kMaxNumDelayableRequestsPerClient)));
  InitializeScheduler();
  scoped_ptr<TestRequest> high(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> high2(NewRequest("http://host/high", net::HIGHEST));
  scoped_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  scoped_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  scoped_ptr<TestRequest> low3(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(high2->started());
  EXPECT_FALSE(low->started());
  EXPECT_FALSE(low2->started());
  EXPECT_FALSE(low3->started());

  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low->started());
  EXPECT_TRUE(low2->started());
  EXPECT_FALSE(low3->started());

  high2.reset();
  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low3->started());
}

TEST_F(ResourceSchedulerTest, TwentyMaxNumDelayableRequestsPerClient) {
  // Do not exceed 20 low-priority requests to be in flight across all hosts
  // at any point in time.
  const int kDeferLateScripts = 0;
  const int kIncreaseFontPriority = 0;
  const int kIncreaseAsyncScriptPriority = 0;
  const int kEnablePriorityIncrease = 0;
  const int kEnableLayoutBlockingThreshold = 0;
  const int kLayoutBlockingThreshold = 0;
  const int kMaxNumDelayableWhileLayoutBlocking = 1;
  const int kMaxNumDelayableRequestsPerClient = 20;
  ASSERT_TRUE(InitializeFieldTrials(base::StringPrintf(
      "ResourcePriorities/LayoutBlocking_%d%d%d%d%d_%d_%d_%d/",
      kDeferLateScripts,
      kIncreaseFontPriority,
      kIncreaseAsyncScriptPriority,
      kEnablePriorityIncrease,
      kEnableLayoutBlockingThreshold,
      kLayoutBlockingThreshold,
      kMaxNumDelayableWhileLayoutBlocking,
      kMaxNumDelayableRequestsPerClient)));
  InitializeScheduler();

  // Only load low priority resources if there's a body.
  scheduler()->OnWillInsertBody(kChildId, kRouteId);

  // Queue requests from different hosts until the total limit is reached.
  ScopedVector<TestRequest> lows_different_host;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient; ++i) {
    string url = "http://host" + base::IntToString(i) + "/low";
    lows_different_host.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows_different_host[i]->started());
  }

  scoped_ptr<TestRequest> last_different_host(NewRequest("http://host_new/last",
                                                        net::LOWEST));
  EXPECT_FALSE(last_different_host->started());
}

TEST_F(ResourceSchedulerTest,
    TwentyMaxNumDelayableRequestsPerClientWithEverythingEnabled) {
  // Do not exceed 20 low-priority requests to be in flight across all hosts
  // at any point in time and make sure it still works correctly when the other
  // options are toggled.
  const int kDeferLateScripts = 1;
  const int kIncreaseFontPriority = 1;
  const int kIncreaseAsyncScriptPriority = 1;
  const int kEnablePriorityIncrease = 1;
  const int kEnableLayoutBlockingThreshold = 1;
  const int kLayoutBlockingThreshold = 1;
  const int kMaxNumDelayableWhileLayoutBlocking = 1;
  const int kMaxNumDelayableRequestsPerClient = 20;
  ASSERT_TRUE(InitializeFieldTrials(base::StringPrintf(
      "ResourcePriorities/LayoutBlocking_%d%d%d%d%d_%d_%d_%d/",
      kDeferLateScripts,
      kIncreaseFontPriority,
      kIncreaseAsyncScriptPriority,
      kEnablePriorityIncrease,
      kEnableLayoutBlockingThreshold,
      kLayoutBlockingThreshold,
      kMaxNumDelayableWhileLayoutBlocking,
      kMaxNumDelayableRequestsPerClient)));
  InitializeScheduler();

  // Only load low priority resources if there's a body.
  scheduler()->OnWillInsertBody(kChildId, kRouteId);

  // Queue requests from different hosts until the total limit is reached.
  ScopedVector<TestRequest> lows_different_host;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient; ++i) {
    string url = "http://host" + base::IntToString(i) + "/low";
    lows_different_host.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows_different_host[i]->started());
  }

  scoped_ptr<TestRequest> last_different_host(NewRequest("http://host_new/last",
                                                         net::LOWEST));
  EXPECT_FALSE(last_different_host->started());
}

}  // unnamed namespace

}  // namespace content
