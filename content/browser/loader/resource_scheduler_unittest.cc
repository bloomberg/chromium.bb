// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_scheduler.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_render_view_host_factory.h"
#include "content/test/test_web_contents.h"
#include "net/base/host_port_pair.h"
#include "net/base/request_priority.h"
#include "net/http/http_server_properties_impl.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/latency/latency_info.h"
#include "url/scheme_host_port.h"

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

const char kPrioritySupportedRequestsDelayable[] =
    "PrioritySupportedRequestsDelayable";

const char kNetworkSchedulerYielding[] = "NetworkSchedulerYielding";
const int kMaxRequestsBeforeYielding = 5;  // sync with .cc.

class TestRequest : public ResourceThrottle::Delegate {
 public:
  TestRequest(std::unique_ptr<net::URLRequest> url_request,
              std::unique_ptr<ResourceThrottle> throttle,
              ResourceScheduler* scheduler)
      : started_(false),
        url_request_(std::move(url_request)),
        throttle_(std::move(throttle)),
        scheduler_(scheduler) {
    throttle_->set_delegate_for_testing(this);
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
  // ResourceThrottle::Delegate interface:
  void CancelAndIgnore() override {}
  void CancelWithError(int error_code) override {}
  void Resume() override { started_ = true; }

 private:
  bool started_;
  std::unique_ptr<net::URLRequest> url_request_;
  std::unique_ptr<ResourceThrottle> throttle_;
  ResourceScheduler* scheduler_;
};

class CancelingTestRequest : public TestRequest {
 public:
  CancelingTestRequest(std::unique_ptr<net::URLRequest> url_request,
                       std::unique_ptr<ResourceThrottle> throttle,
                       ResourceScheduler* scheduler)
      : TestRequest(std::move(url_request), std::move(throttle), scheduler) {}

  void set_request_to_cancel(std::unique_ptr<TestRequest> request_to_cancel) {
    request_to_cancel_ = std::move(request_to_cancel);
  }

 private:
  void Resume() override {
    TestRequest::Resume();
    request_to_cancel_.reset();
  }

  std::unique_ptr<TestRequest> request_to_cancel_;
};

class FakeResourceContext : public ResourceContext {
 private:
  net::HostResolver* GetHostResolver() override { return NULL; }
  net::URLRequestContext* GetRequestContext() override { return NULL; }
};

class ResourceSchedulerTest : public testing::Test {
 protected:
  ResourceSchedulerTest() : field_trial_list_(nullptr) {
    InitializeScheduler();
    context_.set_http_server_properties(&http_server_properties_);
    context_.set_network_quality_estimator(&network_quality_estimator_);
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

    scheduler_->OnClientCreated(kChildId, kRouteId,
                                &network_quality_estimator_);
    scheduler_->OnClientCreated(kBackgroundChildId, kBackgroundRouteId,
                                &network_quality_estimator_);
  }

  void CleanupScheduler() {
    if (scheduler_) {
      scheduler_->OnClientDeleted(kChildId, kRouteId);
      scheduler_->OnClientDeleted(kBackgroundChildId, kBackgroundRouteId);
    }
  }

  std::unique_ptr<net::URLRequest> NewURLRequestWithChildAndRoute(
      const char* url,
      net::RequestPriority priority,
      int child_id,
      int route_id) {
    std::unique_ptr<net::URLRequest> url_request(context_.CreateRequest(
        GURL(url), priority, nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));
    return url_request;
  }

  std::unique_ptr<net::URLRequest> NewURLRequest(
      const char* url,
      net::RequestPriority priority) {
    return NewURLRequestWithChildAndRoute(url, priority, kChildId, kRouteId);
  }

  std::unique_ptr<TestRequest> NewRequestWithRoute(
      const char* url,
      net::RequestPriority priority,
      int route_id) {
    return NewRequestWithChildAndRoute(url, priority, kChildId, route_id);
  }

  std::unique_ptr<TestRequest> NewRequestWithChildAndRoute(
      const char* url,
      net::RequestPriority priority,
      int child_id,
      int route_id) {
    return GetNewTestRequest(url, priority, child_id, route_id, true);
  }

  std::unique_ptr<TestRequest> NewRequest(const char* url,
                                          net::RequestPriority priority) {
    return NewRequestWithChildAndRoute(url, priority, kChildId, kRouteId);
  }

  std::unique_ptr<TestRequest> NewBackgroundRequest(
      const char* url,
      net::RequestPriority priority) {
    return NewRequestWithChildAndRoute(
        url, priority, kBackgroundChildId, kBackgroundRouteId);
  }

  std::unique_ptr<TestRequest> NewSyncRequest(const char* url,
                                              net::RequestPriority priority) {
    return NewSyncRequestWithChildAndRoute(url, priority, kChildId, kRouteId);
  }

  std::unique_ptr<TestRequest> NewBackgroundSyncRequest(
      const char* url,
      net::RequestPriority priority) {
    return NewSyncRequestWithChildAndRoute(
        url, priority, kBackgroundChildId, kBackgroundRouteId);
  }

  std::unique_ptr<TestRequest> NewSyncRequestWithChildAndRoute(
      const char* url,
      net::RequestPriority priority,
      int child_id,
      int route_id) {
    return GetNewTestRequest(url, priority, child_id, route_id, false);
  }

  std::unique_ptr<TestRequest> GetNewTestRequest(const char* url,
                                                 net::RequestPriority priority,
                                                 int child_id,
                                                 int route_id,
                                                 bool is_async) {
    std::unique_ptr<net::URLRequest> url_request(
        NewURLRequestWithChildAndRoute(url, priority, child_id, route_id));
    std::unique_ptr<ResourceThrottle> throttle(scheduler_->ScheduleRequest(
        child_id, route_id, is_async, url_request.get()));
    auto request = base::MakeUnique<TestRequest>(
        std::move(url_request), std::move(throttle), scheduler());
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

  void InitializeMaxDelayableRequestsExperiment(
      base::test::ScopedFeatureList* scoped_feature_list,
      bool enabled) {
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
    const char kTrialName[] = "TrialName";
    const char kGroupName[] = "GroupName";
    const char kMaxDelayableRequestsNetworkOverride[] =
        "MaxDelayableRequestsNetworkOverride";

    std::map<std::string, std::string> params;
    params["MaxEffectiveConnectionType"] = "2G";
    params["MaxBDPKbits1"] = "130";
    params["MaxDelayableRequests1"] = "2";
    params["MaxBDPKbits2"] = "160";
    params["MaxDelayableRequests2"] = "4";

    base::AssociateFieldTrialParams(kTrialName, kGroupName, params);
    base::FieldTrial* field_trial =
        base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);

    std::unique_ptr<base::FeatureList> feature_list(
        base::MakeUnique<base::FeatureList>());
    feature_list->RegisterFieldTrialOverride(
        kMaxDelayableRequestsNetworkOverride,
        enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                : base::FeatureList::OVERRIDE_DISABLE_FEATURE,
        field_trial);
    scoped_feature_list->InitWithFeatureList(std::move(feature_list));
  }

  void ReadConfigTestHelper(size_t num_bdp_ranges,
                            const std::string& max_ect_string) {
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
    const char kTrialName[] = "TrialName";
    const char kGroupName[] = "GroupName";
    const char kMaxDelayableRequestsNetworkOverride[] =
        "MaxDelayableRequestsNetworkOverride";

    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
    base::test::ScopedFeatureList scoped_feature_list;
    std::map<std::string, std::string> params;
    params["MaxEffectiveConnectionType"] = max_ect_string;
    for (size_t bdp_range_index = 1; bdp_range_index <= num_bdp_ranges;
         bdp_range_index++) {
      std::string index_str = base::SizeTToString(bdp_range_index);
      params["MaxBDPKbits" + index_str] = index_str + "00";
      params["MaxDelayableRequests" + index_str] = index_str + "0";
    }

    base::AssociateFieldTrialParams(kTrialName, kGroupName, params);
    base::FieldTrial* field_trial =
        base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);
    std::unique_ptr<base::FeatureList> feature_list(
        base::MakeUnique<base::FeatureList>());
    feature_list->RegisterFieldTrialOverride(
        kMaxDelayableRequestsNetworkOverride,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, field_trial);
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    ResourceScheduler::MaxRequestsForBDPRanges bdp_ranges =
        ResourceScheduler::GetMaxDelayableRequestsExperimentConfigForTests();

    // Check that the configuration was parsed and stored correctly.
    ASSERT_EQ(bdp_ranges.size(), num_bdp_ranges);
    for (size_t bdp_range_index = 1; bdp_range_index <= num_bdp_ranges;
         bdp_range_index++) {
      EXPECT_EQ(bdp_ranges[bdp_range_index - 1].max_bdp_kbits,
                (int64_t)(bdp_range_index * 100));
      EXPECT_EQ(bdp_ranges[bdp_range_index - 1].max_requests,
                bdp_range_index * 10u);
    }

    net::EffectiveConnectionType max_ect;
    net::GetEffectiveConnectionTypeForName(max_ect_string, &max_ect);
    EXPECT_EQ(
        ResourceScheduler::GetMaxDelayableRequestsExperimentMaxECTForTests(),
        max_ect);
  }

  ResourceScheduler* scheduler() {
    return scheduler_.get();
  }

  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<ResourceScheduler> scheduler_;
  base::MockTimer* mock_timer_;
  net::HttpServerPropertiesImpl http_server_properties_;
  net::TestNetworkQualityEstimator network_quality_estimator_;
  net::TestURLRequestContext context_;
  base::FieldTrialList field_trial_list_;
};

TEST_F(ResourceSchedulerTest, OneIsolatedLowRequest) {
  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/1", net::LOWEST));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, OneLowLoadsUntilBodyInserted) {
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());

  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(low2->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, OneLowLoadsUntilCriticalComplete) {
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
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

TEST_F(ResourceSchedulerTest, MediumDoesNotBlockCriticalComplete) {
  // kLayoutBlockingPriorityThreshold determines what priority level above which
  // requests are considered layout-blocking and must be completed before the
  // critical loading period is complete.  It is currently set to net::MEDIUM.
  std::unique_ptr<TestRequest> medium(
      NewRequest("http://host/low", net::MEDIUM));
  std::unique_ptr<TestRequest> lowest(
      NewRequest("http://host/lowest", net::LOWEST));
  std::unique_ptr<TestRequest> lowest2(
      NewRequest("http://host/lowest", net::LOWEST));
  EXPECT_TRUE(medium->started());
  EXPECT_TRUE(lowest->started());
  EXPECT_FALSE(lowest2->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(lowest2->started());
}

TEST_F(ResourceSchedulerTest, SchedulerYieldsWithFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(kNetworkSchedulerYielding, "");
  InitializeScheduler();

  // Use spdy so that we don't throttle.
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  // Add enough async requests that the last one should yield.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (int i = 0; i < kMaxRequestsBeforeYielding + 1; ++i)
    requests.push_back(NewRequest("http://host/higher", net::HIGHEST));

  // Verify that the number of requests before yielding started.
  for (int i = 0; i < kMaxRequestsBeforeYielding; ++i)
    EXPECT_TRUE(requests[i]->started());

  // The next async request should have yielded.
  EXPECT_FALSE(requests[kMaxRequestsBeforeYielding]->started());

  // Verify that with time the yielded request eventually runs.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(requests[kMaxRequestsBeforeYielding]->started());
}

TEST_F(ResourceSchedulerTest, SchedulerDoesNotYieldForSyncRequests) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(kNetworkSchedulerYielding, "");
  InitializeScheduler();

  // Use spdy so that we don't throttle.
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  // Add enough async requests that the last one should yield.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (int i = 0; i < kMaxRequestsBeforeYielding + 1; ++i)
    requests.push_back(NewRequest("http://host/higher", net::HIGHEST));

  // Add a sync requests.
  requests.push_back(NewSyncRequest("http://host/higher", net::HIGHEST));

  // Verify that the number of requests before yielding started.
  for (int i = 0; i < kMaxRequestsBeforeYielding; ++i)
    EXPECT_TRUE(requests[i]->started());

  // The next async request should have yielded.
  EXPECT_FALSE(requests[kMaxRequestsBeforeYielding]->started());

  // The next sync request should have started even though async is yielding.
  EXPECT_TRUE(requests[kMaxRequestsBeforeYielding + 1]->started());

  // Verify that with time the yielded request eventually runs.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(requests[kMaxRequestsBeforeYielding]->started());
}

TEST_F(ResourceSchedulerTest, SchedulerDoesNotYieldForAlternativeSchemes) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(kNetworkSchedulerYielding, "");
  InitializeScheduler();

  // Use spdy so that we don't throttle.
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  // Add enough async requests that the last one should yield.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (int i = 0; i < kMaxRequestsBeforeYielding + 1; ++i)
    requests.push_back(NewRequest("http://host/higher", net::HIGHEST));

  // Add a non-http request.
  requests.push_back(NewRequest("zzz://host/higher", net::HIGHEST));

  // Verify that the number of requests before yielding started.
  for (int i = 0; i < kMaxRequestsBeforeYielding; ++i)
    EXPECT_TRUE(requests[i]->started());

  // The next async request should have yielded.
  EXPECT_FALSE(requests[kMaxRequestsBeforeYielding]->started());

  // The non-http(s) request should have started even though async is
  // yielding.
  EXPECT_TRUE(requests[kMaxRequestsBeforeYielding + 1]->started());

  // Verify that with time the yielded request eventually runs.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(requests[kMaxRequestsBeforeYielding]->started());
}

TEST_F(ResourceSchedulerTest, SchedulerDoesNotYieldWithFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("", kNetworkSchedulerYielding);
  InitializeScheduler();

  // Use spdy so that we don't throttle.
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  // Add enough async requests that the last one would yield if yielding were
  // enabled.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (int i = 0; i < kMaxRequestsBeforeYielding + 1; ++i)
    requests.push_back(NewRequest("http://host/higher", net::HIGHEST));

  // Verify that none of the requests yield.
  for (int i = 0; i < kMaxRequestsBeforeYielding + 1; ++i)
    EXPECT_TRUE(requests[i]->started());
}

TEST_F(ResourceSchedulerTest, OneLowLoadsUntilBodyInsertedExceptSpdy) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("",
                                          kPrioritySupportedRequestsDelayable);
  InitializeScheduler();

  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> low_spdy(
      NewRequest("https://spdyhost/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());
  EXPECT_TRUE(low_spdy->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest,
       OneLowLoadsUntilBodyInsertedEvenSpdyWhenDelayable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(kPrioritySupportedRequestsDelayable,
                                          "");

  InitializeScheduler();
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> low_spdy(
      NewRequest("https://spdyhost/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());
  EXPECT_FALSE(low_spdy->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, SpdyLowBlocksOtherLowUntilBodyInserted) {
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low_spdy(
      NewRequest("https://spdyhost/low", net::LOWEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low_spdy->started());
  EXPECT_FALSE(low->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low->started());
}

TEST_F(ResourceSchedulerTest, NavigationResetsState) {
  base::HistogramTester histogram_tester;
  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  scheduler()->OnNavigate(kChildId, kRouteId);

  {
    std::unique_ptr<TestRequest> high(
        NewRequest("http://host/high", net::HIGHEST));
    std::unique_ptr<TestRequest> low(
        NewRequest("http://host/low", net::LOWEST));
    std::unique_ptr<TestRequest> low2(
        NewRequest("http://host/low", net::LOWEST));
    EXPECT_TRUE(high->started());
    EXPECT_TRUE(low->started());
    EXPECT_FALSE(low2->started());
  }

  histogram_tester.ExpectTotalCount("ResourceScheduler.RequestsCount.All", 2);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("ResourceScheduler.RequestsCount.All"),
      testing::ElementsAre(base::Bucket(1, 1), base::Bucket(2, 1)));

  histogram_tester.ExpectTotalCount("ResourceScheduler.RequestsCount.Delayable",
                                    2);
  histogram_tester.ExpectTotalCount(
      "ResourceScheduler.RequestsCount.NonDelayable", 2);
  histogram_tester.ExpectTotalCount(
      "ResourceScheduler.RequestsCount.TotalLayoutBlocking", 2);

  histogram_tester.ExpectUniqueSample(
      "ResourceScheduler.PeakDelayableRequestsInFlight.LayoutBlocking", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "ResourceScheduler.PeakDelayableRequestsInFlight.NonDelayable", 1, 1);
}

TEST_F(ResourceSchedulerTest, BackgroundRequestStartsImmediately) {
  const int route_id = 0;  // Indicates a background request.
  std::unique_ptr<TestRequest> request(
      NewRequestWithRoute("http://host/1", net::LOWEST, route_id));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, MoreThanOneHighRequestBlocksDelayableRequests) {
  // If there are more than kInFlightNonDelayableRequestCountThreshold (=1)
  // high-priority / non-delayable requests, block all low priority fetches and
  // allow them through one at a time once the number of high priority requests
  // drops.
  std::unique_ptr<TestRequest> high1(
      NewRequest("http://host/high1", net::HIGHEST));
  std::unique_ptr<TestRequest> high2(
      NewRequest("http://host/high2", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high1->started());
  EXPECT_TRUE(high2->started());
  EXPECT_FALSE(low->started());
  EXPECT_FALSE(low2->started());

  high1.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());
}

TEST_F(ResourceSchedulerTest, CancelOtherRequestsWhileResuming) {
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low1(
      NewRequest("http://host/low1", net::LOWEST));

  std::unique_ptr<net::URLRequest> url_request(
      NewURLRequest("http://host/low2", net::LOWEST));
  std::unique_ptr<ResourceThrottle> throttle(scheduler()->ScheduleRequest(
      kChildId, kRouteId, true, url_request.get()));
  std::unique_ptr<CancelingTestRequest> low2(new CancelingTestRequest(
      std::move(url_request), std::move(throttle), scheduler()));
  low2->Start();

  std::unique_ptr<TestRequest> low3(
      NewRequest("http://host/low3", net::LOWEST));
  low2->set_request_to_cancel(std::move(low3));
  std::unique_ptr<TestRequest> low4(
      NewRequest("http://host/low4", net::LOWEST));

  EXPECT_TRUE(high->started());
  EXPECT_FALSE(low2->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low1->started());
  EXPECT_TRUE(low2->started());
  EXPECT_TRUE(low4->started());
}

TEST_F(ResourceSchedulerTest, LimitedNumberOfDelayableRequestsInFlight) {
  // The yielding feature will sometimes yield requests before they get a
  // chance to start, which conflicts this test. So disable the feature.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("", kNetworkSchedulerYielding);

  // We only load low priority resources if there's a body.
  scheduler()->OnWillInsertBody(kChildId, kRouteId);

  // Throw in one high priority request to make sure that's not a factor.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high->started());

  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.
  const int kMaxNumDelayableRequestsPerHost = 6;
  std::vector<std::unique_ptr<TestRequest>> lows_singlehost;
  // Queue up to the per-host limit (we subtract the current high-pri request).
  for (int i = 0; i < kMaxNumDelayableRequestsPerHost - 1; ++i) {
    string url = "http://host/low" + base::IntToString(i);
    lows_singlehost.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows_singlehost[i]->started());
  }

  std::unique_ptr<TestRequest> second_last_singlehost(
      NewRequest("http://host/last", net::LOWEST));
  std::unique_ptr<TestRequest> last_singlehost(
      NewRequest("http://host/s_last", net::LOWEST));

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
  std::vector<std::unique_ptr<TestRequest>> lows_different_host;
  base::RunLoop().RunUntilIdle();
  for (int i = 0; i < expected_slots_left; ++i) {
    string url = "http://host" + base::IntToString(i) + "/low";
    lows_different_host.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows_different_host[i]->started());
  }

  std::unique_ptr<TestRequest> last_different_host(
      NewRequest("http://host_new/last", net::LOWEST));
  EXPECT_FALSE(last_different_host->started());
}

TEST_F(ResourceSchedulerTest, RaisePriorityAndStart) {
  // Dummies to enforce scheduling.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/req", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::LOWEST));
  EXPECT_FALSE(request->started());

  ChangeRequestPriority(request.get(), net::HIGHEST);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, RaisePriorityInQueue) {
  // Dummies to enforce scheduling.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::IDLE));
  std::unique_ptr<TestRequest> idle(NewRequest("http://host/idle", net::IDLE));
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  ChangeRequestPriority(request.get(), net::LOWEST);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.
  std::vector<std::unique_ptr<TestRequest>> lows;
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
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::LOWEST));
  std::unique_ptr<TestRequest> idle(NewRequest("http://host/idle", net::IDLE));
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
  std::vector<std::unique_ptr<TestRequest>> lows;
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
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::LOWEST));
  std::unique_ptr<TestRequest> idle(NewRequest("http://host/idle", net::IDLE));
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.
  std::vector<std::unique_ptr<TestRequest>> lows;
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
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.
  std::vector<std::unique_ptr<TestRequest>> lows;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient; ++i) {
    string url = "http://host/low" + base::IntToString(i);
    lows.push_back(NewRequest(url.c_str(), net::IDLE));
  }

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::IDLE));
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
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("chrome-extension://req", net::LOWEST));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, SpdyProxySchedulesImmediately) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("",
                                          kPrioritySupportedRequestsDelayable);
  InitializeScheduler();

  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::IDLE));
  EXPECT_FALSE(request->started());

  scheduler()->OnReceivedSpdyProxiedHttpResponse(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request->started());

  std::unique_ptr<TestRequest> after(
      NewRequest("http://host/after", net::IDLE));
  EXPECT_TRUE(after->started());
}

TEST_F(ResourceSchedulerTest, SpdyProxyDelayable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(kPrioritySupportedRequestsDelayable,
                                          "");
  InitializeScheduler();

  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::IDLE));
  EXPECT_FALSE(request->started());

  scheduler()->OnReceivedSpdyProxiedHttpResponse(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());

  std::unique_ptr<TestRequest> after(
      NewRequest("http://host/after", net::IDLE));
  EXPECT_FALSE(after->started());
}

TEST_F(ResourceSchedulerTest, NewSpdyHostInDelayableRequests) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("",
                                          kPrioritySupportedRequestsDelayable);
  InitializeScheduler();

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.

  std::unique_ptr<TestRequest> low1_spdy(
      NewRequest("http://spdyhost1:8080/low", net::LOWEST));
  // Cancel a request after we learn the server supports SPDY.
  std::vector<std::unique_ptr<TestRequest>> lows;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient - 1; ++i) {
    string url = "http://host" + base::IntToString(i) + "/low";
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }
  std::unique_ptr<TestRequest> low1(NewRequest("http://host/low", net::LOWEST));
  EXPECT_FALSE(low1->started());
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("http", "spdyhost1", 8080), true);
  low1_spdy.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low1->started());

  low1.reset();
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<TestRequest> low2_spdy(
      NewRequest("http://spdyhost2:8080/low", net::IDLE));
  // Reprioritize a request after we learn the server supports SPDY.
  EXPECT_TRUE(low2_spdy->started());
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("http", "spdyhost2", 8080), true);
  ChangeRequestPriority(low2_spdy.get(), net::LOWEST);
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, NewDelayableSpdyHostInDelayableRequests) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(kPrioritySupportedRequestsDelayable,
                                          "");
  InitializeScheduler();

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.

  std::unique_ptr<TestRequest> low1_spdy(
      NewRequest("http://spdyhost1:8080/low", net::LOWEST));
  // Cancel a request after we learn the server supports SPDY.
  std::vector<std::unique_ptr<TestRequest>> lows;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient - 1; ++i) {
    string url = "http://host" + base::IntToString(i) + "/low";
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }
  std::unique_ptr<TestRequest> low1(NewRequest("http://host/low", net::LOWEST));
  EXPECT_FALSE(low1->started());
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("http", "spdyhost1", 8080), true);
  low1_spdy.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low1->started());

  low1.reset();
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<TestRequest> low2_spdy(
      NewRequest("http://spdyhost2:8080/low", net::IDLE));
  // Reprioritize a request after we learn the server supports SPDY.
  EXPECT_TRUE(low2_spdy->started());
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("http", "spdyhost2", 8080), true);
  ChangeRequestPriority(low2_spdy.get(), net::LOWEST);
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_FALSE(low2->started());
}

// Async revalidations which are not started when the tab is closed must be
// started at some point, or they will hang around forever and prevent other
// async revalidations to the same URL from being issued.
TEST_F(ResourceSchedulerTest, RequestStartedAfterClientDeleted) {
  scheduler_->OnClientCreated(kChildId2, kRouteId2,
                              &network_quality_estimator_);
  std::unique_ptr<TestRequest> high(NewRequestWithChildAndRoute(
      "http://host/high", net::HIGHEST, kChildId2, kRouteId2));
  std::unique_ptr<TestRequest> lowest1(NewRequestWithChildAndRoute(
      "http://host/lowest", net::LOWEST, kChildId2, kRouteId2));
  std::unique_ptr<TestRequest> lowest2(NewRequestWithChildAndRoute(
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
  scheduler_->OnClientCreated(kChildId2, kRouteId2,
                              &network_quality_estimator_);
  std::unique_ptr<TestRequest> high(NewRequestWithChildAndRoute(
      "http://host/high", net::HIGHEST, kChildId2, kRouteId2));
  const int kMaxNumDelayableRequestsPerClient = 10;
  std::vector<std::unique_ptr<TestRequest>> delayable_requests;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient + 1; ++i) {
    delayable_requests.push_back(NewRequestWithChildAndRoute(
        "http://host/lowest", net::LOWEST, kChildId2, kRouteId2));
  }
  std::unique_ptr<TestRequest> lowest(NewRequestWithChildAndRoute(
      "http://host/lowest", net::LOWEST, kChildId2, kRouteId2));
  EXPECT_FALSE(lowest->started());

  scheduler_->OnClientDeleted(kChildId2, kRouteId2);
  high.reset();
  delayable_requests.clear();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(lowest->started());
}

// Tests that the maximum number of delayable requests is overridden when the
// experiment is enabled and not when it is disabled. The BDP buckets are
// correct and the effective connection type is also in the configuration
// bucket.
TEST_F(ResourceSchedulerTest, RequestLimitOverrideConfigTest) {
  for (bool experiment_status : {true, false}) {
    base::test::ScopedFeatureList scoped_feature_list;
    InitializeMaxDelayableRequestsExperiment(&scoped_feature_list,
                                             experiment_status);

    // Set BDP to 120 kbits, which lies in the first configuration bucket. Set
    // the effective connection type to Slow-2G, which is slower than the
    // threshold configured in |InitializeMaxDelayableRequestsExperiment|. Needs
    // to be done before initializing the scheduler because the client is
    // created on the call to |InitializeScheduler|, which is where the initial
    // limits for the delayable requests in flight are computed.
    network_quality_estimator_.set_bandwidth_delay_product_kbits(120);
    network_quality_estimator_.set_effective_connection_type(
        net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
    // Initialize the scheduler.
    InitializeScheduler();

    // For 2G, the typical values of RTT and bandwidth should result in the
    // override taking effect with the experiment enabled. For this case, the
    // new limit is 2. The limit will matter only once the page has a body,
    // since delayable requests are not loaded before that.
    scheduler()->OnWillInsertBody(kChildId, kRouteId);

    // Throw in one high priority request to ensure that it does not matter once
    // a body exists.
    std::unique_ptr<TestRequest> high2(
        NewRequest("http://host/high2", net::HIGHEST));
    EXPECT_TRUE(high2->started());

    // Should match the configuration set by
    // |InitializeMaxDelayableRequestsExperiment|
    const int kOverriddenNumRequests = 2;

    std::vector<std::unique_ptr<TestRequest>> lows_singlehost;
    // Queue the maximum number of delayable requests that should be started
    // before the resource scheduler starts throttling delayable requests.
    for (int i = 0; i < kOverriddenNumRequests; ++i) {
      std::string url = "http://host/low" + base::IntToString(i);
      lows_singlehost.push_back(NewRequest(url.c_str(), net::LOWEST));
      EXPECT_TRUE(lows_singlehost[i]->started());
    }

    std::unique_ptr<TestRequest> second_last_singlehost(
        NewRequest("http://host/s_last", net::LOWEST));
    std::unique_ptr<TestRequest> last_singlehost(
        NewRequest("http://host/last", net::LOWEST));

    if (experiment_status) {
      // Experiment enabled, hence requests should be limited.
      // Second last should not start because there are |kOverridenNumRequests|
      // delayable requests already in-flight.
      EXPECT_FALSE(second_last_singlehost->started());

      // Completion of a delayable request must result in starting of the
      // second-last request.
      lows_singlehost.erase(lows_singlehost.begin());
      base::RunLoop().RunUntilIdle();
      EXPECT_TRUE(second_last_singlehost->started());
      EXPECT_FALSE(last_singlehost->started());

      // Completion of another delayable request must result in starting of the
      // last request.
      lows_singlehost.erase(lows_singlehost.begin());
      base::RunLoop().RunUntilIdle();
      EXPECT_TRUE(last_singlehost->started());
    } else {
      // Requests should start because the default limit is 10.
      EXPECT_TRUE(second_last_singlehost->started());
      EXPECT_TRUE(last_singlehost->started());
    }
  }
}

// Test that the limit is not overridden when the effective connection type is
// not equal to any of the values provided in the experiment configuration.
TEST_F(ResourceSchedulerTest, RequestLimitOverrideOutsideECTRange) {
  for (net::EffectiveConnectionType ect :
       {net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
        net::EFFECTIVE_CONNECTION_TYPE_OFFLINE,
        net::EFFECTIVE_CONNECTION_TYPE_3G, net::EFFECTIVE_CONNECTION_TYPE_4G}) {
    base::test::ScopedFeatureList scoped_feature_list;
    InitializeMaxDelayableRequestsExperiment(&scoped_feature_list, true);
    // Set BDP to 120 kbits, which lies in the configuration bucket. Set the
    // effective connection type to a value for which the experiment should not
    // be run.
    network_quality_estimator_.set_bandwidth_delay_product_kbits(120);
    network_quality_estimator_.set_effective_connection_type(ect);
    InitializeScheduler();

    // The limit will matter only once the page has a body, since delayable
    // requests are not loaded before that.
    scheduler()->OnWillInsertBody(kChildId, kRouteId);

    // Throw in one high priority request to ensure that it does not matter once
    // a body exists.
    std::unique_ptr<TestRequest> high(
        NewRequest("http://host/high", net::HIGHEST));
    EXPECT_TRUE(high->started());

    // Should be in sync with resource_scheduler.cc.
    const int kMaxNumDelayableRequestsPerClient = 10;

    std::vector<std::unique_ptr<TestRequest>> lows_singlehost;
    // Queue up to the maximum limit. Use different host names to prevent the
    // per host limit from kicking in.
    for (int i = 0; i < kMaxNumDelayableRequestsPerClient; ++i) {
      // Keep unique hostnames to prevent the per host limit from kicking in.
      std::string url = "http://host" + base::IntToString(i) + "/low";
      lows_singlehost.push_back(NewRequest(url.c_str(), net::LOWEST));
      EXPECT_TRUE(lows_singlehost[i]->started());
    }

    std::unique_ptr<TestRequest> last_singlehost(
        NewRequest("http://host/last", net::LOWEST));

    // Last should not start because the maximum requests that can be in-flight
    // have already started.
    EXPECT_FALSE(last_singlehost->started());
  }
}

// Test that the limit is not overridden when the effective connection type is
// valid, but the bandwidth delay product (BDP) does not lie in one of the
// buckets provided in the configuration.
TEST_F(ResourceSchedulerTest, RequestLimitOverrideConfigOutsideBDPRange) {
  base::test::ScopedFeatureList scoped_feature_list;
  InitializeMaxDelayableRequestsExperiment(&scoped_feature_list, true);
  // The BDP should lie outside the provided ranges. Here, the BDP is set to
  // 200, which lies outside the configuration BDP buckets.
  // The effective connection type is set to Slow-2G.
  network_quality_estimator_.set_bandwidth_delay_product_kbits(200);
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  InitializeScheduler();

  // The limit should be the default limit, which is 10 delayable requests
  // in-flight. The limit will matter only once the page has a body, since
  // delayable requests are not loaded before that.
  scheduler()->OnWillInsertBody(kChildId, kRouteId);

  // Throw in one high priority request to ensure that it does not matter once
  // a body exists.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high->started());

  // Should be in sync with resource_scheduler.cc.
  const int kMaxNumDelayableRequestsPerClient = 10;

  std::vector<std::unique_ptr<TestRequest>> lows_singlehost;
  // Queue up to the maximum limit. Use different host names to prevent the
  // per host limit from kicking in.
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient; ++i) {
    // Keep unique hostnames to prevent the per host limit from kicking in.
    std::string url = "http://host" + base::IntToString(i) + "/low";
    lows_singlehost.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows_singlehost[i]->started());
  }

  std::unique_ptr<TestRequest> last_singlehost(
      NewRequest("http://host/last", net::LOWEST));

  // Last should not start because the maximum requests that can be in-flight
  // have already started.
  EXPECT_FALSE(last_singlehost->started());
}

// Test that a change in network conditions midway during loading does not
// change the behavior of the resource scheduler.
TEST_F(ResourceSchedulerTest, RequestLimitOverrideFixedForPageLoad) {
  base::test::ScopedFeatureList scoped_feature_list;
  InitializeMaxDelayableRequestsExperiment(&scoped_feature_list, true);
  // BDP value is in range for which the limit is overridden to 2. The
  // effective connection type is set to Slow-2G.
  network_quality_estimator_.set_bandwidth_delay_product_kbits(120);
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  InitializeScheduler();

  // The limit will matter only once the page has a body, since delayable
  // requests are not loaded before that.
  scheduler()->OnWillInsertBody(kChildId, kRouteId);

  // Throw in one high priority request to ensure that it does not matter once
  // a body exists.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high->started());

  // Should be based on the value set by
  // |InitializeMaxDelayableRequestsExperiment| for the given range.
  const int kOverriddenNumRequests = 2;

  std::vector<std::unique_ptr<TestRequest>> lows_singlehost;
  // Queue up to the overridden limit.
  for (int i = 0; i < kOverriddenNumRequests; ++i) {
    // Keep unique hostnames to prevent the per host limit from kicking in.
    std::string url = "http://host" + base::IntToString(i) + "/low";
    lows_singlehost.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows_singlehost[i]->started());
  }

  std::unique_ptr<TestRequest> second_last_singlehost(
      NewRequest("http://host/slast", net::LOWEST));

  // This new request should not start because the limit has been reached.
  EXPECT_FALSE(second_last_singlehost->started());
  lows_singlehost.erase(lows_singlehost.begin());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(second_last_singlehost->started());

  // Change the BDP to go outside the experiment buckets and change the network
  // type to 2G. This should not affect the limit calculated at the beginning of
  // the page load.
  network_quality_estimator_.set_bandwidth_delay_product_kbits(50);
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  std::unique_ptr<TestRequest> last_singlehost(
      NewRequest("http://host/last", net::LOWEST));

  // Last should not start because the limit should not have changed.
  EXPECT_FALSE(last_singlehost->started());
}

// Test that when the network quality changes such that the new limit is lower,
// and an |OnNavigate| event occurs, the new delayable requests don't start
// until the number of requests in flight have gone below the new limit.
TEST_F(ResourceSchedulerTest, RequestLimitReducedAcrossPageLoads) {
  base::test::ScopedFeatureList scoped_feature_list;
  InitializeMaxDelayableRequestsExperiment(&scoped_feature_list, true);
  // BDP value is in range for which the limit is overridden to 4. The
  // effective connection type is set to Slow-2G.
  network_quality_estimator_.set_bandwidth_delay_product_kbits(150);
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  InitializeScheduler();

  // The limit will matter only once the page has a body, since delayable
  // requests are not loaded before that.
  scheduler()->OnWillInsertBody(kChildId, kRouteId);

  // Throw in one high priority request to ensure that it does not matter once
  // a body exists.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high->started());

  // The number of delayable requests allowed for the first page load.
  const int kNumDelayableHigh = 4;
  // The number of delayable requests allowed for the second page load.
  const int kNumDelayableLow = 2;

  std::vector<std::unique_ptr<TestRequest>> delayable_first_page;
  // Queue up to the overridden limit.
  for (int i = 0; i < kNumDelayableHigh; ++i) {
    // Keep unique hostnames to prevent the per host limit from kicking in.
    std::string url = "http://host" + base::IntToString(i) + "/low1";
    delayable_first_page.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(delayable_first_page[i]->started());
  }
  // Change the network quality so that the BDP value is in range for which the
  // limit is overridden to 2. The effective connection type is set to
  // Slow-2G.
  network_quality_estimator_.set_bandwidth_delay_product_kbits(120);
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  // Trigger a navigation event which will recompute limits. Also insert a body,
  // because the limit matters only after the body exists.
  scheduler()->OnNavigate(kChildId, kRouteId);
  scheduler()->OnWillInsertBody(kChildId, kRouteId);

  // Ensure that high priority requests still start.
  std::unique_ptr<TestRequest> high2(
      NewRequest("http://host/high2", net::HIGHEST));
  EXPECT_TRUE(high->started());

  // Generate requests from second page. None of them should start because the
  // new limit is 2 and there are already 4 requests in flight.
  std::vector<std::unique_ptr<TestRequest>> delayable_second_page;
  for (int i = 0; i < kNumDelayableLow; ++i) {
    // Keep unique hostnames to prevent the per host limit from kicking in.
    std::string url = "http://host" + base::IntToString(i) + "/low2";
    delayable_second_page.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_FALSE(delayable_second_page[i]->started());
  }

  // Finish 2 requests from first page load.
  for (int i = 0; i < kNumDelayableLow; ++i) {
    delayable_first_page.pop_back();
  }
  base::RunLoop().RunUntilIdle();

  // Nothing should start because there are already 2 requests in flight.
  for (int i = 0; i < kNumDelayableLow; ++i) {
    EXPECT_FALSE(delayable_second_page[i]->started());
  }

  // Remove all requests from the first page.
  delayable_first_page.clear();
  base::RunLoop().RunUntilIdle();

  // Check that the requests from page 2 have started, since now there are 2
  // empty slots.
  for (int i = 0; i < kNumDelayableLow; ++i) {
    EXPECT_TRUE(delayable_second_page[i]->started());
  }
}

// Test that a configuration without any BDP range is read correctly. In this
// case, the resource scheduler will fall back to the default limit.
TEST_F(ResourceSchedulerTest, ReadValidConfigTest0) {
  ReadConfigTestHelper(0, "2G");
}

// Test that a configuration with 1 BDP range is read correctly.
TEST_F(ResourceSchedulerTest, ReadValidConfigTest1) {
  ReadConfigTestHelper(1, "2G");
}

// Test that a configuration with 2 BDP ranges is read correctly.
TEST_F(ResourceSchedulerTest, ReadValidConfigTest2) {
  ReadConfigTestHelper(2, "2G");
}

// Test that a configuration with 5 BDP ranges is read correctly.
TEST_F(ResourceSchedulerTest, ReadValidConfigTest5) {
  ReadConfigTestHelper(5, "2G");
}

// Test that a configuration with bad strings does not break the parser, and
// the parser stops reading the configuration after it encounters the first
// missing index.
TEST_F(ResourceSchedulerTest, ReadInvalidConfigTest) {
  base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
  const char kTrialName[] = "TrialName";
  const char kGroupName[] = "GroupName";
  const char kMaxDelayableRequestsNetworkOverride[] =
      "MaxDelayableRequestsNetworkOverride";

  base::test::ScopedFeatureList scoped_feature_list;
  std::map<std::string, std::string> params;
  params["MaxEffectiveConnectionType"] = "2G";
  // Skip configuration parameters for index 2 to test that the parser stops
  // when it cannot find the parameters for an index.
  for (int bdp_range_index : {1, 3, 4}) {
    std::string index_str = base::IntToString(bdp_range_index);
    params["MaxBDPKbits" + index_str] = index_str + "00";
    params["MaxDelayableRequests" + index_str] = index_str + "0";
  }
  // Add some bad configuration strigs to ensure that the parser does not break.
  params["BadConfigParam1"] = "100";
  params["BadConfigParam2"] = "100";

  base::AssociateFieldTrialParams(kTrialName, kGroupName, params);
  base::FieldTrial* field_trial =
      base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);
  std::unique_ptr<base::FeatureList> feature_list(
      base::MakeUnique<base::FeatureList>());
  feature_list->RegisterFieldTrialOverride(
      kMaxDelayableRequestsNetworkOverride,
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, field_trial);
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  ResourceScheduler::MaxRequestsForBDPRanges bdp_ranges =
      ResourceScheduler::GetMaxDelayableRequestsExperimentConfigForTests();

  // Only the first configuration parameter must be read because a match was not
  // found for index 2. The configuration parameters with index 3 and 4 must be
  // ignored, even though they are valid configuration parameters.
  EXPECT_EQ(bdp_ranges.size(), 1u);
  EXPECT_EQ(bdp_ranges[0].max_bdp_kbits, 100);
  EXPECT_EQ(bdp_ranges[0].max_requests, 10u);
}

// Test that the maximum effective connection type is read correctly when it is
// set to "Slow-2G".
TEST_F(ResourceSchedulerTest, ReadMaxECTForExperimentTestSlow2G) {
  ReadConfigTestHelper(3, "Slow-2G");
}

// Test that the maximum effective connection type is read correctly when it is
// set to "4G".
TEST_F(ResourceSchedulerTest, ReadMaxECTForExperimentTest4G) {
  ReadConfigTestHelper(3, "4G");
}

}  // unnamed namespace

}  // namespace content
