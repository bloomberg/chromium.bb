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
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
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

// Sync below with cc file.
const char kPrioritySupportedRequestsDelayable[] =
    "PrioritySupportedRequestsDelayable";
const char kHeadPrioritySupportedRequestsDelayable[] =
    "HeadPriorityRequestsDelayable";
const char kNetworkSchedulerYielding[] = "NetworkSchedulerYielding";
const size_t kMaxNumDelayableRequestsPerHostPerClient = 6;

void ConfigureYieldFieldTrial(
    int max_requests_before_yielding,
    int max_yield_ms,
    base::test::ScopedFeatureList* scoped_feature_list) {
  const std::string kTrialName = "TrialName";
  const std::string kGroupName = "GroupName";  // Value not used
  const std::string kNetworkSchedulerYielding = "NetworkSchedulerYielding";

  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);

  std::map<std::string, std::string> params;
  params["MaxRequestsBeforeYieldingParam"] =
      base::IntToString(max_requests_before_yielding);
  params["MaxYieldMs"] = base::IntToString(max_yield_ms);
  base::FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
      kTrialName, kGroupName, params);

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->RegisterFieldTrialOverride(
      kNetworkSchedulerYielding, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial.get());
  scoped_feature_list->InitWithFeatureList(std::move(feature_list));
}

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
  net::HostResolver* GetHostResolver() override { return nullptr; }
  net::URLRequestContext* GetRequestContext() override { return nullptr; }
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
  void InitializeScheduler(bool enabled = true) {
    CleanupScheduler();

    // Destroys previous scheduler, also destroys any previously created
    // mock_timer_.
    scheduler_.reset(new ResourceScheduler(enabled));

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
    auto request = std::make_unique<TestRequest>(
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

  void RequestLimitOverrideConfigTestHelper(bool experiment_status) {
    base::test::ScopedFeatureList scoped_feature_list;
    InitializeThrottleDelayableExperiment(&scoped_feature_list,
                                          experiment_status, 0.0);

    // Set the effective connection type to Slow-2G, which is slower than the
    // threshold configured in |InitializeMaxDelayableRequestsExperiment|. Needs
    // to be done before initializing the scheduler because the client is
    // created on the call to |InitializeScheduler|, which is where the initial
    // limits for the delayable requests in flight are computed.
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

  void InitializeThrottleDelayableExperiment(
      base::test::ScopedFeatureList* scoped_feature_list,
      bool lower_delayable_count_enabled,
      double non_delayable_weight) {
    std::map<std::string, std::string> params;
    bool experiment_enabled = false;
    if (lower_delayable_count_enabled) {
      experiment_enabled = true;
      params["EffectiveConnectionType1"] = "Slow-2G";
      params["MaxDelayableRequests1"] = "2";
      params["NonDelayableWeight1"] = "0.0";

      params["EffectiveConnectionType2"] = "3G";
      params["MaxDelayableRequests2"] = "4";
      params["NonDelayableWeight2"] = "0.0";
    }

    if (non_delayable_weight > 0.0) {
      experiment_enabled = true;
      params["EffectiveConnectionType1"] = "Slow-2G";
      if (params["MaxDelayableRequests1"] == "")
        params["MaxDelayableRequests1"] = "10";
      params["NonDelayableWeight1"] =
          base::NumberToString(non_delayable_weight);
    }

    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
    const char kTrialName[] = "TrialName";
    const char kGroupName[] = "GroupName";

    ASSERT_TRUE(
        base::AssociateFieldTrialParams(kTrialName, kGroupName, params));
    base::FieldTrial* field_trial =
        base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);
    ASSERT_TRUE(field_trial);

    std::unique_ptr<base::FeatureList> feature_list(
        std::make_unique<base::FeatureList>());
    feature_list->RegisterFieldTrialOverride(
        "ThrottleDelayable",
        experiment_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                           : base::FeatureList::OVERRIDE_DISABLE_FEATURE,
        field_trial);
    scoped_feature_list->InitWithFeatureList(std::move(feature_list));

    ResourceScheduler::ParamsForNetworkQualityContainer
        params_network_quality_container =
            ResourceScheduler::GetParamsForNetworkQualityContainerForTests();

    if (!lower_delayable_count_enabled && non_delayable_weight <= 0.0) {
      ASSERT_EQ(0u, params_network_quality_container.size());
      return;
    }

    // Check that the configuration was parsed and stored correctly.
    ASSERT_EQ(lower_delayable_count_enabled ? 2u : 1u,
              params_network_quality_container.size());

    EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
              params_network_quality_container[0].effective_connection_type);
    EXPECT_EQ(non_delayable_weight > 0.0 ? 10u : 2u,
              params_network_quality_container[0].max_delayable_requests);
    EXPECT_EQ(non_delayable_weight > 0.0 ? non_delayable_weight : 0.0,
              params_network_quality_container[0].non_delayable_weight);

    if (lower_delayable_count_enabled) {
      EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_3G,
                params_network_quality_container[1].effective_connection_type);
      EXPECT_EQ(4u, params_network_quality_container[1].max_delayable_requests);
      EXPECT_EQ(0.0, params_network_quality_container[1].non_delayable_weight);
    }
  }

  void ReadConfigTestHelper(size_t num_ranges) {
    const char kTrialName[] = "TrialName";
    const char kGroupName[] = "GroupName";
    const char kThrottleDelayable[] = "ThrottleDelayable";

    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
    base::test::ScopedFeatureList scoped_feature_list;
    std::map<std::string, std::string> params;
    for (size_t index = 1; index <= num_ranges; index++) {
      std::string index_str = base::NumberToString(index);
      params["EffectiveConnectionType" + index_str] =
          net::GetNameForEffectiveConnectionType(
              static_cast<net::EffectiveConnectionType>(1 + index));
      params["MaxDelayableRequests" + index_str] = index_str + "0";
      params["NonDelayableWeight" + index_str] = "0";
    }

    base::AssociateFieldTrialParams(kTrialName, kGroupName, params);
    base::FieldTrial* field_trial =
        base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);
    std::unique_ptr<base::FeatureList> feature_list(
        std::make_unique<base::FeatureList>());
    feature_list->RegisterFieldTrialOverride(
        kThrottleDelayable, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        field_trial);
    scoped_feature_list.InitWithFeatureList(std::move(feature_list));

    ResourceScheduler::ParamsForNetworkQualityContainer
        params_network_quality_container =
            ResourceScheduler::GetParamsForNetworkQualityContainerForTests();

    // Check that the configuration was parsed and stored correctly.
    ASSERT_EQ(params_network_quality_container.size(), num_ranges);
    for (size_t index = 1; index <= num_ranges; index++) {
      EXPECT_EQ(1 + index, params_network_quality_container[index - 1]
                               .effective_connection_type);
      EXPECT_EQ(
          index * 10u,
          params_network_quality_container[index - 1].max_delayable_requests);
      EXPECT_EQ(
          0, params_network_quality_container[index - 1].non_delayable_weight);
    }
  }

  void NonDelayableThrottlesDelayableHelper(double non_delayable_weight) {
    base::test::ScopedFeatureList scoped_feature_list;
    // Should be in sync with .cc.
    const int kDefaultMaxNumDelayableRequestsPerClient = 10;
    // Initialize the experiment.
    InitializeThrottleDelayableExperiment(&scoped_feature_list, false,
                                          non_delayable_weight);
    network_quality_estimator_.set_effective_connection_type(
        net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

    InitializeScheduler();
    // Limit will only trigger after the page has a body.
    scheduler()->OnWillInsertBody(kChildId, kRouteId);
    // Start one non-delayable request.
    std::unique_ptr<TestRequest> non_delayable_request(
        NewRequest("http://host/medium", net::MEDIUM));
    // Start |kDefaultMaxNumDelayableRequestsPerClient - 1 *
    // |non_delayable_weight|  delayable requests. They should all start.
    std::vector<std::unique_ptr<TestRequest>> delayable_requests;
    for (int i = 0;
         i < kDefaultMaxNumDelayableRequestsPerClient - non_delayable_weight;
         ++i) {
      delayable_requests.push_back(NewRequest(
          base::StringPrintf("http://host%d/low", i).c_str(), net::LOWEST));
      EXPECT_TRUE(delayable_requests.back()->started());
    }
    // The next delayable request should not start.
    std::unique_ptr<TestRequest> last_low(
        NewRequest("http://lasthost/low", net::LOWEST));
    EXPECT_FALSE(last_low->started());
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

TEST_F(ResourceSchedulerTest, SchedulerYieldsOnSpdy) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(kNetworkSchedulerYielding, "");
  InitializeScheduler();

  // The second low-priority request should yield.
  scheduler_->SetMaxRequestsBeforeYieldingForTesting(1);

  // Set a custom yield time.
  scheduler_->SetYieldTimeForTesting(base::TimeDelta::FromMilliseconds(42));

  // Use a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  scheduler_->SetTaskRunnerForTesting(task_runner);

  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  std::unique_ptr<TestRequest> request(
      NewRequest("https://spdyhost/low", net::LOWEST));
  std::unique_ptr<TestRequest> request2(
      NewRequest("https://spdyhost/low", net::LOWEST));
  std::unique_ptr<TestRequest> request3(
      NewRequest("https://spdyhost/low", net::LOWEST));

  // Just before the yield task runs, only the first request should have
  // started.
  task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(41));
  EXPECT_TRUE(request->started());
  EXPECT_FALSE(request2->started());
  EXPECT_FALSE(request3->started());

  // Yield is done, run the next task.
  task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  EXPECT_TRUE(request2->started());
  EXPECT_FALSE(request3->started());

  // Just before the yield task runs, only the first two requests should have
  // started.
  task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(41));
  EXPECT_FALSE(request3->started());

  // Yield is done, run the next task.
  task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  EXPECT_TRUE(request3->started());
}

// Same as SchedulerYieldsOnSpdy but uses FieldTrial Parameters for
// configuration.
TEST_F(ResourceSchedulerTest, SchedulerYieldFieldTrialParams) {
  base::test::ScopedFeatureList scoped_feature_list;

  ConfigureYieldFieldTrial(1 /* requests before yielding */,
                           42 /* yield time */, &scoped_feature_list);
  InitializeScheduler();

  // Make sure the parameters were properly set.
  EXPECT_EQ(42, scheduler_->yield_time().InMilliseconds());
  EXPECT_EQ(1, scheduler_->max_requests_before_yielding());

  // Use a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  scheduler_->SetTaskRunnerForTesting(task_runner);

  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  std::unique_ptr<TestRequest> request(
      NewRequest("https://spdyhost/low", net::LOWEST));
  std::unique_ptr<TestRequest> request2(
      NewRequest("https://spdyhost/low", net::LOWEST));

  // Just before the yield task runs, only the first request should have
  // started.
  task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(41));
  EXPECT_TRUE(request->started());
  EXPECT_FALSE(request2->started());

  // Yield is done, run the next task.
  task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  EXPECT_TRUE(request2->started());
}

TEST_F(ResourceSchedulerTest, YieldingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("", kNetworkSchedulerYielding);
  InitializeScheduler();

  // We're setting a yield parameter, but no yielding will happen since it's
  // disabled.
  scheduler_->SetMaxRequestsBeforeYieldingForTesting(1);

  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  std::unique_ptr<TestRequest> request(
      NewRequest("https://spdyhost/low", net::LOWEST));
  std::unique_ptr<TestRequest> request2(
      NewRequest("https://spdyhost/low", net::LOWEST));
  EXPECT_TRUE(request->started());
  EXPECT_TRUE(request2->started());
}

TEST_F(ResourceSchedulerTest, SchedulerDoesNotYieldH1) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(kNetworkSchedulerYielding, "");
  InitializeScheduler();

  // Use a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  scheduler_->SetTaskRunnerForTesting(task_runner);

  // Yield after each request.
  scheduler_->SetMaxRequestsBeforeYieldingForTesting(1);
  scheduler_->SetYieldTimeForTesting(base::TimeDelta::FromMilliseconds(42));

  std::unique_ptr<TestRequest> request(
      NewRequest("https://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> request2(
      NewRequest("https://host/low", net::LOWEST));

  EXPECT_TRUE(request->started());
  EXPECT_FALSE(request2->started());

  // Finish the first task so that the second can start.
  request = nullptr;

  // Run tasks without advancing time, if there were yielding the next task
  // wouldn't start.
  task_runner->RunUntilIdle();

  // The next task started, so there was no yielding.
  EXPECT_TRUE(request2->started());
}

TEST_F(ResourceSchedulerTest, SchedulerDoesNotYieldAltSchemes) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(kNetworkSchedulerYielding, "");
  InitializeScheduler();

  // Yield after each request.
  scheduler_->SetMaxRequestsBeforeYieldingForTesting(1);
  scheduler_->SetYieldTimeForTesting(base::TimeDelta::FromMilliseconds(42));

  std::unique_ptr<TestRequest> request(
      NewRequest("yyy://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> request2(
      NewRequest("zzz://host/low", net::LOWEST));

  EXPECT_TRUE(request->started());
  EXPECT_TRUE(request2->started());
}

TEST_F(ResourceSchedulerTest, SchedulerDoesNotYieldSyncRequests) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(kNetworkSchedulerYielding, "");
  InitializeScheduler();

  // The second low-priority request should yield.
  scheduler_->SetMaxRequestsBeforeYieldingForTesting(1);

  // Use spdy so that we don't throttle.
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  std::unique_ptr<TestRequest> request(
      NewRequest("https://spdyhost/low", net::LOWEST));
  std::unique_ptr<TestRequest> request2(
      NewRequest("https://spdyhost/low", net::LOWEST));  // yields

  // Add a synchronous request, it shouldn't yield.
  std::unique_ptr<TestRequest> sync_request(
      NewSyncRequest("http://spdyhost/low", net::LOWEST));

  EXPECT_TRUE(request->started());
  EXPECT_FALSE(request2->started());
  EXPECT_TRUE(sync_request->started());  // The sync request started.

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request2->started());
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
  EXPECT_TRUE(low_spdy->started());
}

TEST_F(ResourceSchedulerTest, MaxRequestsPerHostForSpdyWhenNotDelayable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("",
                                          kPrioritySupportedRequestsDelayable);

  InitializeScheduler();
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  // Add more than max-per-host low-priority requests.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i)
    requests.push_back(NewRequest("https://spdyhost/low", net::LOWEST));

  // No throttling.
  for (const auto& request : requests)
    EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, MaxRequestsPerHostForSpdyWhenDelayable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      kPrioritySupportedRequestsDelayable,
      kHeadPrioritySupportedRequestsDelayable);

  InitializeScheduler();
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  // Body has been reached.
  scheduler()->OnWillInsertBody(kChildId, kRouteId);

  // Add more than max-per-host low-priority requests.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i)
    requests.push_back(NewRequest("https://spdyhost/low", net::LOWEST));

  // Only kMaxNumDelayableRequestsPerHostPerClient in body.
  for (size_t i = 0; i < requests.size(); ++i) {
    if (i < kMaxNumDelayableRequestsPerHostPerClient)
      EXPECT_TRUE(requests[i]->started());
    else
      EXPECT_FALSE(requests[i]->started());
  }
}

TEST_F(ResourceSchedulerTest, MaxRequestsPerHostForSpdyWhenHeadDelayable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      kHeadPrioritySupportedRequestsDelayable,
      kPrioritySupportedRequestsDelayable);

  InitializeScheduler();
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  // Body has been reached.
  scheduler()->OnWillInsertBody(kChildId, kRouteId);

  // Add more than max-per-host low-priority requests.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i)
    requests.push_back(NewRequest("https://spdyhost/low", net::LOWEST));

  // No throttling.
  for (const auto& request : requests)
    EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, ThrottlesHeadWhenHeadDelayable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      kHeadPrioritySupportedRequestsDelayable,
      kPrioritySupportedRequestsDelayable);

  InitializeScheduler();
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  // Add more than max-per-host low-priority requests.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i)
    requests.push_back(NewRequest("https://spdyhost/low", net::LOWEST));

  // While in head, only one low-priority request is allowed.
  for (size_t i = 0u; i < requests.size(); ++i) {
    if (i == 0u)
      EXPECT_TRUE(requests[i]->started());
    else
      EXPECT_FALSE(requests[i]->started());
  }

  // Body has been reached.
  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();

  // No throttling.
  for (const auto& request : requests)
    EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, MaxRequestsPerHostForSpdyProxyWhenNotDelayable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("",
                                          kPrioritySupportedRequestsDelayable);

  InitializeScheduler();

  // Body has been reached.
  scheduler()->OnWillInsertBody(kChildId, kRouteId);

  // Add more than max-per-host low-priority requests.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i)
    requests.push_back(NewRequest("http://host/low", net::LOWEST));

  // Now the scheduler realizes these requests are for a spdy proxy.
  scheduler()->OnReceivedSpdyProxiedHttpResponse(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();

  // No throttling.
  for (const auto& request : requests)
    EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, MaxRequestsPerHostForSpdyProxyWhenDelayable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      kPrioritySupportedRequestsDelayable,
      kHeadPrioritySupportedRequestsDelayable);

  InitializeScheduler();

  // Body has been reached.
  scheduler()->OnWillInsertBody(kChildId, kRouteId);

  // Add more than max-per-host low-priority requests.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i)
    requests.push_back(NewRequest("http://host/low", net::LOWEST));

  // Now the scheduler realizes these requests are for a spdy proxy.
  scheduler()->OnReceivedSpdyProxiedHttpResponse(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();

  // Only kMaxNumDelayableRequestsPerHostPerClient in body.
  for (size_t i = 0; i < requests.size(); ++i) {
    if (i < kMaxNumDelayableRequestsPerHostPerClient)
      EXPECT_TRUE(requests[i]->started());
    else
      EXPECT_FALSE(requests[i]->started());
  }
}

TEST_F(ResourceSchedulerTest, MaxRequestsPerHostForSpdyProxyWhenHeadDelayable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      kHeadPrioritySupportedRequestsDelayable,
      kPrioritySupportedRequestsDelayable);

  InitializeScheduler();

  // Body has been reached.
  scheduler()->OnWillInsertBody(kChildId, kRouteId);

  // Add more than max-per-host low-priority requests.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i)
    requests.push_back(NewRequest("http://host/low", net::LOWEST));

  // Now the scheduler realizes these requests are for a spdy proxy.
  scheduler()->OnReceivedSpdyProxiedHttpResponse(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();

  // No throttling.
  for (const auto& request : requests)
    EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, ThrottlesHeadForSpdyProxyWhenHeadDelayable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      kHeadPrioritySupportedRequestsDelayable,
      kPrioritySupportedRequestsDelayable);

  InitializeScheduler();

  // Add more than max-per-host low-priority requests.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i)
    requests.push_back(NewRequest("http://host/low", net::LOWEST));

  // Now the scheduler realizes these requests are for a spdy proxy.
  scheduler()->OnReceivedSpdyProxiedHttpResponse(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();

  // While in head, only one low-priority request is allowed.
  for (size_t i = 0u; i < requests.size(); ++i) {
    if (i == 0u)
      EXPECT_TRUE(requests[i]->started());
    else
      EXPECT_FALSE(requests[i]->started());
  }

  // Body has been reached.
  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();

  // No throttling.
  for (const auto& request : requests)
    EXPECT_TRUE(request->started());
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

  const int kDefaultMaxNumDelayableRequestsPerClient =
      10;  // Should match the .cc.
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
  int expected_slots_left = kDefaultMaxNumDelayableRequestsPerClient -
                            kMaxNumDelayableRequestsPerHost;
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

  const int kDefaultMaxNumDelayableRequestsPerClient =
      10;  // Should match the .cc.
  std::vector<std::unique_ptr<TestRequest>> lows;
  for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient - 1; ++i) {
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

  const int kDefaultMaxNumDelayableRequestsPerClient =
      10;  // Should match the .cc.
  // 2 fewer filler requests: 1 for the "low" dummy at the start, and 1 for the
  // one at the end, which will be tested.
  const int kNumFillerRequests = kDefaultMaxNumDelayableRequestsPerClient - 2;
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

  const int kDefaultMaxNumDelayableRequestsPerClient =
      10;  // Should match the .cc.
  std::vector<std::unique_ptr<TestRequest>> lows;
  for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient; ++i) {
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

  const int kDefaultMaxNumDelayableRequestsPerClient =
      10;  // Should match the .cc.
  std::vector<std::unique_ptr<TestRequest>> lows;
  for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient; ++i) {
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
  const int kDefaultMaxNumDelayableRequestsPerClient =
      10;  // Should match the .cc.

  std::unique_ptr<TestRequest> low1_spdy(
      NewRequest("http://spdyhost1:8080/low", net::LOWEST));
  // Cancel a request after we learn the server supports SPDY.
  std::vector<std::unique_ptr<TestRequest>> lows;
  for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient - 1; ++i) {
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
  const int kDefaultMaxNumDelayableRequestsPerClient =
      10;  // Should match the .cc.

  std::unique_ptr<TestRequest> low1_spdy(
      NewRequest("http://spdyhost1:8080/low", net::LOWEST));
  // Cancel a request after we learn the server supports SPDY.
  std::vector<std::unique_ptr<TestRequest>> lows;
  for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient - 1; ++i) {
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
  const int kDefaultMaxNumDelayableRequestsPerClient = 10;
  std::vector<std::unique_ptr<TestRequest>> delayable_requests;
  for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient + 1; ++i) {
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
// experiment is enabled.
TEST_F(ResourceSchedulerTest, RequestLimitOverrideEnabled) {
  RequestLimitOverrideConfigTestHelper(true);
}

// Tests that the maximum number of delayable requests is not overridden when
// the experiment is disabled.
TEST_F(ResourceSchedulerTest, RequestLimitOverrideDisabled) {
  RequestLimitOverrideConfigTestHelper(false);
}

// Test that the limit is not overridden when the effective connection type is
// not equal to any of the values provided in the experiment configuration.
TEST_F(ResourceSchedulerTest, RequestLimitOverrideOutsideECTRange) {
  base::test::ScopedFeatureList scoped_feature_list;
  InitializeThrottleDelayableExperiment(&scoped_feature_list, true, 0.0);
  InitializeScheduler();
  for (net::EffectiveConnectionType ect :
       {net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
        net::EFFECTIVE_CONNECTION_TYPE_OFFLINE,
        net::EFFECTIVE_CONNECTION_TYPE_4G}) {
    // Set the effective connection type to a value for which the experiment
    // should not be run.
    network_quality_estimator_.set_effective_connection_type(ect);

    // The limit will matter only once the page has a body, since delayable
    // requests are not loaded before that.
    scheduler()->OnNavigate(kChildId, kRouteId);
    scheduler()->OnWillInsertBody(kChildId, kRouteId);

    // Throw in one high priority request to ensure that it does not matter once
    // a body exists.
    std::unique_ptr<TestRequest> high(
        NewRequest("http://host/high", net::HIGHEST));
    EXPECT_TRUE(high->started());

    // Should be in sync with resource_scheduler.cc.
    const int kDefaultMaxNumDelayableRequestsPerClient = 10;

    std::vector<std::unique_ptr<TestRequest>> lows_singlehost;
    // Queue up to the maximum limit. Use different host names to prevent the
    // per host limit from kicking in.
    for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient; ++i) {
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

// Test that a change in network conditions midway during loading does not
// change the behavior of the resource scheduler.
TEST_F(ResourceSchedulerTest, RequestLimitOverrideFixedForPageLoad) {
  base::test::ScopedFeatureList scoped_feature_list;
  InitializeThrottleDelayableExperiment(&scoped_feature_list, true, 0.0);
  // ECT value is in range for which the limit is overridden to 2.
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  InitializeScheduler();

  // The limit will matter only once the page has a body, since delayable
  // requests are not loaded before that.
  scheduler()->OnNavigate(kChildId, kRouteId);
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

  // Change the ECT to go outside the experiment buckets and change the network
  // type to 4G. This should not affect the limit calculated at the beginning of
  // the page load.
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_4G);
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<TestRequest> last_singlehost(
      NewRequest("http://host/last", net::LOWEST));

  // Last should not start because the limit should not have changed.
  EXPECT_FALSE(last_singlehost->started());

  // The limit should change when there is a new page navigation.
  scheduler()->OnNavigate(kChildId, kRouteId);
  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(last_singlehost->started());
}

// Test that when the network quality changes such that the new limit is lower,
// and an |OnNavigate| event occurs, the new delayable requests don't start
// until the number of requests in flight have gone below the new limit.
TEST_F(ResourceSchedulerTest, RequestLimitReducedAcrossPageLoads) {
  base::test::ScopedFeatureList scoped_feature_list;
  InitializeThrottleDelayableExperiment(&scoped_feature_list, true, 0.0);
  // ECT value is in range for which the limit is overridden to 4.
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_3G);
  InitializeScheduler();

  // The limit will matter only once the page has a body, since delayable
  // requests are not loaded before that.
  scheduler()->OnNavigate(kChildId, kRouteId);
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
  // Change the network quality so that the ECT value is in range for which the
  // limit is overridden to 2. The effective connection type is set to
  // Slow-2G.
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
  // new limit is |kNumDelayableLow| and there are already |kNumDelayableHigh|
  // requests in flight.
  std::vector<std::unique_ptr<TestRequest>> delayable_second_page;
  for (int i = 0; i < kNumDelayableLow; ++i) {
    // Keep unique hostnames to prevent the per host limit from kicking in.
    std::string url = "http://host" + base::IntToString(i) + "/low2";
    delayable_second_page.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_FALSE(delayable_second_page[i]->started());
  }

  // Finish 2 requests from first page load.
  for (int i = 0; i < kNumDelayableHigh - kNumDelayableLow; ++i) {
    delayable_first_page.pop_back();
  }
  base::RunLoop().RunUntilIdle();

  // Nothing should start because there are already |kNumDelayableLow| requests
  // in flight.
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

  // No new delayable request should start since there are already
  // |kNumDelayableLow| requests in flight.
  std::string url =
      "http://host" + base::IntToString(kNumDelayableLow) + "/low3";
  delayable_second_page.push_back(NewRequest(url.c_str(), net::LOWEST));
  EXPECT_FALSE(delayable_second_page.back()->started());
}

// Test that a configuration without any ECT ranges is read correctly. In this
// case, the resource scheduler will fall back to the default limit.
TEST_F(ResourceSchedulerTest, ReadValidConfigTest0) {
  ReadConfigTestHelper(0);
}

// Test that a configuration with 1 range is read correctly.
TEST_F(ResourceSchedulerTest, ReadValidConfigTest1) {
  ReadConfigTestHelper(1);
}

// Test that a configuration with 2 ranges is read correctly.
TEST_F(ResourceSchedulerTest, ReadValidConfigTest2) {
  ReadConfigTestHelper(2);
}

// Test that a configuration with 3 ranges is read correctly.
TEST_F(ResourceSchedulerTest, ReadValidConfigTest3) {
  ReadConfigTestHelper(3);
}

// Test that a configuration with bad strings does not break the parser, and
// the parser stops reading the configuration after it encounters the first
// missing index.
TEST_F(ResourceSchedulerTest, ReadInvalidConfigTest) {
  base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
  const char kTrialName[] = "TrialName";
  const char kGroupName[] = "GroupName";
  const char kThrottleDelayable[] = "ThrottleDelayable";

  base::test::ScopedFeatureList scoped_feature_list;
  std::map<std::string, std::string> params;
  // Skip configuration parameters for index 2 to test that the parser stops
  // when it cannot find the parameters for an index.
  for (int range_index : {1, 3, 4}) {
    std::string index_str = base::IntToString(range_index);
    params["EffectiveConnectionType" + index_str] = "Slow-2G";
    params["MaxDelayableRequests" + index_str] = index_str + "0";
    params["NonDelayableWeight" + index_str] = "0";
  }
  // Add some bad configuration strigs to ensure that the parser does not break.
  params["BadConfigParam1"] = "100";
  params["BadConfigParam2"] = "100";

  base::AssociateFieldTrialParams(kTrialName, kGroupName, params);
  base::FieldTrial* field_trial =
      base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);
  std::unique_ptr<base::FeatureList> feature_list(
      std::make_unique<base::FeatureList>());
  feature_list->RegisterFieldTrialOverride(
      kThrottleDelayable, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      field_trial);
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  ResourceScheduler::ParamsForNetworkQualityContainer
      params_network_quality_container =
          ResourceScheduler::GetParamsForNetworkQualityContainerForTests();

  // Only the first configuration parameter must be read because a match was not
  // found for index 2. The configuration parameters with index 3 and 4 must be
  // ignored, even though they are valid configuration parameters.
  EXPECT_EQ(1u, params_network_quality_container.size());
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
            params_network_quality_container[0].effective_connection_type);
  EXPECT_EQ(10u, params_network_quality_container[0].max_delayable_requests);
  EXPECT_EQ(0.0, params_network_quality_container[0].non_delayable_weight);
}

// Test that the default limit is used for delayable requests when the
// experiment is enabled, but the current effective connection type is higher
// than the maximum effective connection type set in the experiment
// configuration.
TEST_F(ResourceSchedulerTest, NonDelayableThrottlesDelayableOutsideECT) {
  base::test::ScopedFeatureList scoped_feature_list;
  const double kNonDelayableWeight = 2.0;
  const int kDefaultMaxNumDelayableRequestsPerClient =
      10;  // Should be in sync with cc.
  // Initialize the experiment with |kNonDelayableWeight| as the weight of
  // non-delayable requests.
  InitializeThrottleDelayableExperiment(&scoped_feature_list, false,
                                        kNonDelayableWeight);
  // Experiment should not run when the effective connection type is faster
  // than 2G.
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_3G);
  // Limit will only trigger after the page has a body.

  InitializeScheduler();
  scheduler()->OnNavigate(kChildId, kRouteId);
  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  // Insert one non-delayable request. This should not affect the number of
  // delayable requests started.
  std::unique_ptr<TestRequest> medium(
      NewRequest("http://host/medium", net::MEDIUM));
  ASSERT_TRUE(medium->started());
  // Start |kDefaultMaxNumDelayableRequestsPerClient| delayable requests and
  // verify that they all started.
  std::vector<std::unique_ptr<TestRequest>> delayable_requests;
  for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient; ++i) {
    delayable_requests.push_back(NewRequest(
        base::StringPrintf("http://host%d/low", i).c_str(), net::LOWEST));
    EXPECT_TRUE(delayable_requests.back()->started());
  }
}

// Test that delayable requests are throttled by the right amount as the number
// of non-delayable requests in-flight change.
TEST_F(ResourceSchedulerTest, NonDelayableThrottlesDelayableVaryNonDelayable) {
  base::test::ScopedFeatureList scoped_feature_list;
  const double kNonDelayableWeight = 2.0;
  const int kDefaultMaxNumDelayableRequestsPerClient =
      10;  // Should be in sync with cc.
  // Initialize the experiment with |kNonDelayableWeight| as the weight of
  // non-delayable requests.
  InitializeThrottleDelayableExperiment(&scoped_feature_list, false,
                                        kNonDelayableWeight);
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  InitializeScheduler();
  // Limit will only trigger after the page has a body.
  scheduler()->OnNavigate(kChildId, kRouteId);
  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  for (int num_non_delayable = 0; num_non_delayable < 10; ++num_non_delayable) {
    base::RunLoop().RunUntilIdle();
    // Start the non-delayable requests.
    std::vector<std::unique_ptr<TestRequest>> non_delayable_requests;
    for (int i = 0; i < num_non_delayable; ++i) {
      non_delayable_requests.push_back(NewRequest(
          base::StringPrintf("http://host%d/medium", i).c_str(), net::MEDIUM));
      ASSERT_TRUE(non_delayable_requests.back()->started());
    }
    // Start |kDefaultMaxNumDelayableRequestsPerClient| - |num_non_delayable| *
    // |kNonDelayableWeight| delayable requests. They should all start.
    std::vector<std::unique_ptr<TestRequest>> delayable_requests;
    for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient -
                            num_non_delayable * kNonDelayableWeight;
         ++i) {
      delayable_requests.push_back(NewRequest(
          base::StringPrintf("http://host%d/low", i).c_str(), net::LOWEST));
      EXPECT_TRUE(delayable_requests.back()->started());
    }
    // The next delayable request should not start.
    std::unique_ptr<TestRequest> last_low(
        NewRequest("http://lasthost/low", net::LOWEST));
    EXPECT_FALSE(last_low->started());
  }
}

// Test that the default limit is used for delayable requests in the presence of
// non-delayable requests when the non-delayable request weight is zero.
TEST_F(ResourceSchedulerTest, NonDelayableThrottlesDelayableWeight0) {
  NonDelayableThrottlesDelayableHelper(0.0);
}

// Test that each non-delayable request in-flight results in the reduction of
// one in the limit of delayable requests in-flight when the non-delayable
// request weight is 1.
TEST_F(ResourceSchedulerTest, NonDelayableThrottlesDelayableWeight1) {
  NonDelayableThrottlesDelayableHelper(1.0);
}

// Test that each non-delayable request in-flight results in the reduction of
// three in the limit of delayable requests in-flight when the non-delayable
// request weight is 3.
TEST_F(ResourceSchedulerTest, NonDelayableThrottlesDelayableWeight3) {
  NonDelayableThrottlesDelayableHelper(3.0);
}

// Test that UMA counts are recorded for the number of delayable requests
// in-flight when a non-delayable request starts.
TEST_F(ResourceSchedulerTest, NumDelayableAtStartOfNonDelayableUMA) {
  std::unique_ptr<base::HistogramTester> histogram_tester(
      new base::HistogramTester);
  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  // Check that 0 is recorded when a non-delayable request starts and there are
  // no delayable requests in-flight.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high->started());
  histogram_tester->ExpectUniqueSample(
      "ResourceScheduler.NumDelayableRequestsInFlightAtStart.NonDelayable", 0,
      1);
  histogram_tester.reset(new base::HistogramTester);
  // Check that nothing is recorded when delayable request is started in the
  // presence of a non-delayable request.
  std::unique_ptr<TestRequest> low1(
      NewRequest("http://host/low1", net::LOWEST));
  EXPECT_TRUE(low1->started());
  histogram_tester->ExpectTotalCount(
      "ResourceScheduler.NumDelayableRequestsInFlightAtStart.NonDelayable", 0);
  // Check that nothing is recorded when a delayable request is started in the
  // presence of another delayable request.
  std::unique_ptr<TestRequest> low2(
      NewRequest("http://host/low2", net::LOWEST));
  histogram_tester->ExpectTotalCount(
      "ResourceScheduler.NumDelayableRequestsInFlightAtStart.NonDelayable", 0);
  // Check that UMA is recorded when a non-delayable startes in the presence of
  // delayable requests and that the correct value is recorded.
  std::unique_ptr<TestRequest> high2(
      NewRequest("http://host/high2", net::HIGHEST));
  histogram_tester->ExpectUniqueSample(
      "ResourceScheduler.NumDelayableRequestsInFlightAtStart.NonDelayable", 2,
      1);
}

TEST_F(ResourceSchedulerTest, SchedulerEnabled) {
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/req", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::LOWEST));

  EXPECT_FALSE(request->started());
}

TEST_F(ResourceSchedulerTest, SchedulerDisabled) {
  InitializeScheduler(false);

  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/req", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::LOWEST));

  // Normally |request| wouldn't start immediately due to the |high| priority
  // request, but when the scheduler is disabled it starts immediately.
  EXPECT_TRUE(request->started());
}

}  // unnamed namespace

}  // namespace content
