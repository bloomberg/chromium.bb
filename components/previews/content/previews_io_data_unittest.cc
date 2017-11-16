// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_io_data.h"

#include <initializer_list>
#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_black_list.h"
#include "components/previews/core/previews_black_list_delegate.h"
#include "components/previews/core/previews_black_list_item.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_logger.h"
#include "components/previews/core/previews_opt_out_store.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/load_flags.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

namespace {

// This method simulates the actual behavior of the passed in callback, which is
// validated in other tests. For simplicity, offline, lite page, and server LoFi
// use the offline previews check. Client LoFi uses a seperate check to verify
// that types are treated differently.
bool IsPreviewFieldTrialEnabled(PreviewsType type) {
  switch (type) {
    case PreviewsType::OFFLINE:
    case PreviewsType::LITE_PAGE:
    case PreviewsType::AMP_REDIRECTION:
      return params::IsOfflinePreviewsEnabled();
    case PreviewsType::LOFI:
      return params::IsClientLoFiEnabled();
    case PreviewsType::NOSCRIPT:
      return params::IsNoScriptPreviewsEnabled();
    case PreviewsType::NONE:
    case PreviewsType::LAST:
      break;
  }
  NOTREACHED();
  return false;
}

// Stub class of PreviewsBlackList to control IsLoadedAndAllowed outcome when
// testing PreviewsIOData.
class TestPreviewsBlackList : public PreviewsBlackList {
 public:
  TestPreviewsBlackList(PreviewsEligibilityReason status,
                        PreviewsBlacklistDelegate* blacklist_delegate)
      : PreviewsBlackList(nullptr,
                          base::MakeUnique<base::DefaultClock>(),
                          blacklist_delegate),
        status_(status) {}
  ~TestPreviewsBlackList() override {}

  // PreviewsBlackList:
  PreviewsEligibilityReason IsLoadedAndAllowed(
      const GURL& url,
      PreviewsType type) const override {
    return status_;
  }

 private:
  PreviewsEligibilityReason status_;
};

// Stub class of PreviewsOptimizationGuide to control IsWhitelisted outcome
// when testing PreviewsIOData.
class TestPreviewsOptimizationGuide : public PreviewsOptimizationGuide {
 public:
  TestPreviewsOptimizationGuide(
      optimization_guide::OptimizationGuideService* optimization_guide_service,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
      : PreviewsOptimizationGuide(optimization_guide_service, io_task_runner) {}
  ~TestPreviewsOptimizationGuide() override {}

  // PreviewsOptimizationGuide:
  bool IsWhitelisted(const net::URLRequest& request,
                     PreviewsType type) const override {
    return request.url().host().compare("whitelisted.example.com") == 0;
  }
};

// Stub class of PreviewsUIService to test logging functionalities in
// PreviewsIOData.
class TestPreviewsUIService : public PreviewsUIService {
 public:
  TestPreviewsUIService(
      PreviewsIOData* previews_io_data,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      std::unique_ptr<PreviewsOptOutStore> previews_opt_out_store,
      std::unique_ptr<PreviewsOptimizationGuide> previews_opt_guide,
      const PreviewsIsEnabledCallback& is_enabled_callback,
      std::unique_ptr<PreviewsLogger> logger)
      : PreviewsUIService(previews_io_data,
                          io_task_runner,
                          std::move(previews_opt_out_store),
                          std::move(previews_opt_guide),
                          is_enabled_callback,
                          std::move(logger)),
        user_blacklisted_(false),
        blacklist_ignored_(false) {}

  // PreviewsUIService:
  void OnNewBlacklistedHost(const std::string& host, base::Time time) override {
    host_blacklisted_ = host;
    host_blacklisted_time_ = time;
  }
  void OnUserBlacklistedStatusChange(bool blacklisted) override {
    user_blacklisted_ = blacklisted;
  }
  void OnBlacklistCleared(base::Time time) override {
    blacklist_cleared_time_ = time;
  }
  void OnIgnoreBlacklistDecisionStatusChanged(bool ignored) override {
    blacklist_ignored_ = ignored;
  }

  // Expose passed in LogPreviewDecision parameters.
  const std::vector<PreviewsEligibilityReason>& decision_reasons() const {
    return decision_reasons_;
  }
  const std::vector<GURL>& decision_urls() const { return decision_urls_; }
  const std::vector<PreviewsType>& decision_types() const {
    return decision_types_;
  }
  const std::vector<base::Time>& decision_times() const {
    return decision_times_;
  }

  // Expose passed in LogPreviewsNavigation parameters.
  const std::vector<GURL>& navigation_urls() const { return navigation_urls_; }
  const std::vector<bool>& navigation_opt_outs() const {
    return navigation_opt_outs_;
  }
  const std::vector<base::Time>& navigation_times() const {
    return navigation_times_;
  }
  const std::vector<PreviewsType>& navigation_types() const {
    return navigation_types_;
  }

  // Expose passed in params for hosts and user blacklist event.
  std::string host_blacklisted() const { return host_blacklisted_; }
  base::Time host_blacklisted_time() const { return host_blacklisted_time_; }
  bool user_blacklisted() const { return user_blacklisted_; }
  base::Time blacklist_cleared_time() const { return blacklist_cleared_time_; }

  // Expose the status of blacklist decisions ignored.
  bool blacklist_ignored() const { return blacklist_ignored_; }

 private:
  // PreviewsUIService:
  void LogPreviewNavigation(const GURL& url,
                            PreviewsType type,
                            bool opt_out,
                            base::Time time) override {
    navigation_urls_.push_back(url);
    navigation_opt_outs_.push_back(opt_out);
    navigation_types_.push_back(type);
    navigation_times_.push_back(time);
  }

  void LogPreviewDecisionMade(PreviewsEligibilityReason reason,
                              const GURL& url,
                              base::Time time,
                              PreviewsType type) override {
    decision_reasons_.push_back(reason);
    decision_urls_.push_back(GURL(url));
    decision_times_.push_back(time);
    decision_types_.push_back(type);
  }

  // Passed in params for blacklist status events.
  std::string host_blacklisted_;
  base::Time host_blacklisted_time_;
  bool user_blacklisted_;
  base::Time blacklist_cleared_time_;

  // Passed in LogPreviewDecision parameters.
  std::vector<PreviewsEligibilityReason> decision_reasons_;
  std::vector<GURL> decision_urls_;
  std::vector<PreviewsType> decision_types_;
  std::vector<base::Time> decision_times_;

  // Passed in LogPreviewsNavigation parameters.
  std::vector<GURL> navigation_urls_;
  std::vector<bool> navigation_opt_outs_;
  std::vector<base::Time> navigation_times_;
  std::vector<PreviewsType> navigation_types_;

  // Whether the blacklist decisions are ignored or not.
  bool blacklist_ignored_;
};

class TestPreviewsIOData : public PreviewsIOData {
 public:
  TestPreviewsIOData(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
      : PreviewsIOData(io_task_runner, ui_task_runner), initialized_(false) {}
  ~TestPreviewsIOData() override {}

  // Whether Initialize was called.
  bool initialized() { return initialized_; }

  // Expose the injecting blacklist method from PreviewsIOData, and inject
  // |blacklist| into |this|.
  void InjectTestBlacklist(std::unique_ptr<PreviewsBlackList> blacklist) {
    SetPreviewsBlacklistForTesting(std::move(blacklist));
  }

 private:
  // Set |initialized_| to true and use base class functionality.
  void InitializeOnIOThread(
      std::unique_ptr<PreviewsOptOutStore> previews_opt_out_store) override {
    initialized_ = true;
    PreviewsIOData::InitializeOnIOThread(std::move(previews_opt_out_store));
  }

  // Whether Initialize was called.
  bool initialized_;
};

void RunLoadCallback(
    LoadBlackListCallback callback,
    std::unique_ptr<BlackListItemMap> black_list_item_map,
    std::unique_ptr<PreviewsBlackListItem> host_indifferent_black_list_item) {
  callback.Run(std::move(black_list_item_map),
               std::move(host_indifferent_black_list_item));
}

class TestPreviewsOptOutStore : public PreviewsOptOutStore {
 public:
  TestPreviewsOptOutStore() {}
  ~TestPreviewsOptOutStore() override {}

 private:
  // PreviewsOptOutStore implementation:
  void AddPreviewNavigation(bool opt_out,
                            const std::string& host_name,
                            PreviewsType type,
                            base::Time now) override {}

  void LoadBlackList(LoadBlackListCallback callback) override {
    std::unique_ptr<BlackListItemMap> black_list_item_map(
        new BlackListItemMap());
    std::unique_ptr<PreviewsBlackListItem> host_indifferent_black_list_item =
        PreviewsBlackList::CreateHostIndifferentBlackListItem();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&RunLoadCallback, callback,
                              base::Passed(&black_list_item_map),
                              base::Passed(&host_indifferent_black_list_item)));
  }

  void ClearBlackList(base::Time begin_time, base::Time end_time) override {}
};

class PreviewsIODataTest : public testing::Test {
 public:
  PreviewsIODataTest()
      : field_trial_list_(nullptr),
        io_data_(scoped_task_environment_.GetMainThreadTaskRunner(),
                 scoped_task_environment_.GetMainThreadTaskRunner()),
        optimization_guide_service_(
            scoped_task_environment_.GetMainThreadTaskRunner()),
        context_(true) {
    context_.set_network_quality_estimator(&network_quality_estimator_);
    context_.Init();

    network_quality_estimator_.set_effective_connection_type(
        net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  }

  ~PreviewsIODataTest() override {
    // TODO(dougarnett) bug 781975: Consider switching to Feature API and
    // ScopedFeatureList (and dropping components/variations dep).
    variations::testing::ClearAllVariationParams();
  }

  void InitializeUIServiceWithoutWaitingForBlackList() {
    ui_service_.reset(new TestPreviewsUIService(
        &io_data_, scoped_task_environment_.GetMainThreadTaskRunner(),
        base::MakeUnique<TestPreviewsOptOutStore>(),
        base::MakeUnique<TestPreviewsOptimizationGuide>(
            &optimization_guide_service_,
            scoped_task_environment_.GetMainThreadTaskRunner()),
        base::Bind(&IsPreviewFieldTrialEnabled),
        base::MakeUnique<PreviewsLogger>()));
  }

  void InitializeUIService() {
    InitializeUIServiceWithoutWaitingForBlackList();
    scoped_task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<net::URLRequest> CreateRequest() const {
    return CreateRequestWithURL(GURL("http://example.com"));
  }

  std::unique_ptr<net::URLRequest> CreateHttpsRequest() const {
    return CreateRequestWithURL(GURL("https://secure.example.com"));
  }

  std::unique_ptr<net::URLRequest> CreateRequestWithURL(const GURL& url) const {
    return context_.CreateRequest(url, net::DEFAULT_PRIORITY, nullptr,
                                  TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  TestPreviewsIOData* io_data() { return &io_data_; }
  TestPreviewsUIService* ui_service() { return ui_service_.get(); }
  net::TestURLRequestContext* context() { return &context_; }
  net::TestNetworkQualityEstimator* network_quality_estimator() {
    return &network_quality_estimator_;
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::FieldTrialList field_trial_list_;
  TestPreviewsIOData io_data_;
  optimization_guide::OptimizationGuideService optimization_guide_service_;
  std::unique_ptr<TestPreviewsUIService> ui_service_;
  net::TestNetworkQualityEstimator network_quality_estimator_;
  net::TestURLRequestContext context_;
};

void CreateFieldTrialWithParams(
    const std::string& trial_name,
    const std::string& group_name,
    std::initializer_list<
        typename std::map<std::string, std::string>::value_type> params) {
  EXPECT_TRUE(base::AssociateFieldTrialParams(trial_name, group_name, params));
  EXPECT_TRUE(base::FieldTrialList::CreateFieldTrial(trial_name, group_name));
}

TEST_F(PreviewsIODataTest, TestInitialization) {
  InitializeUIService();
  // After the outstanding posted tasks have run, |io_data_| should be fully
  // initialized.
  EXPECT_TRUE(io_data()->initialized());
}

// Tests most of the reasons that a preview could be disallowed because of the
// state of the blacklist. Excluded values are USER_RECENTLY_OPTED_OUT,
// USER_BLACKLISTED, HOST_BLACKLISTED. These are internal to the blacklist.
TEST_F(PreviewsIODataTest, TestDisallowPreviewBecauseOfBlackListState) {
  std::unique_ptr<net::URLRequest> request = CreateRequest();
  base::HistogramTester histogram_tester;

  // The blacklist is not created yet.
  EXPECT_FALSE(io_data()->ShouldAllowPreview(*request, PreviewsType::OFFLINE));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.Offline",
      static_cast<int>(PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE), 1);

  InitializeUIServiceWithoutWaitingForBlackList();

  // The blacklist is not created yet.
  EXPECT_FALSE(io_data()->ShouldAllowPreview(*request, PreviewsType::OFFLINE));
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.Offline",
      static_cast<int>(PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE), 2);

  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount("Previews.EligibilityReason.Offline", 2);

  // Return one of the failing statuses from the blacklist; cause the blacklist
  // to not be loaded by clearing the blacklist.
  base::Time now = base::Time::Now();
  io_data()->ClearBlackList(now, now);

  EXPECT_FALSE(io_data()->ShouldAllowPreview(*request, PreviewsType::OFFLINE));
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.Offline",
      static_cast<int>(PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED),
      1);
  variations::testing::ClearAllVariationParams();
}

TEST_F(PreviewsIODataTest, TestDisallowOfflineWhenNetworkQualityUnavailable) {
  InitializeUIService();

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(
      io_data()->ShouldAllowPreview(*CreateRequest(), PreviewsType::OFFLINE));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.Offline",
      static_cast<int>(PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE),
      1);
}

TEST_F(PreviewsIODataTest, TestAllowLitePageWhenNetworkQualityFast) {
  // LoFi and LitePage check NQE on their own.
  InitializeUIService();

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_3G);

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(io_data()->ShouldAllowPreviewAtECT(
      *CreateRequest(), PreviewsType::LITE_PAGE,
      net::EFFECTIVE_CONNECTION_TYPE_4G, std::vector<std::string>()));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.LitePage",
      static_cast<int>(PreviewsEligibilityReason::ALLOWED), 1);
}

TEST_F(PreviewsIODataTest, TestDisallowOfflineWhenNetworkQualityFast) {
  InitializeUIService();

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_3G);
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(
      io_data()->ShouldAllowPreview(*CreateRequest(), PreviewsType::OFFLINE));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.Offline",
      static_cast<int>(PreviewsEligibilityReason::NETWORK_NOT_SLOW), 1);
}

TEST_F(PreviewsIODataTest, TestDisallowOfflineOnReload) {
  InitializeUIService();

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  std::unique_ptr<net::URLRequest> request = CreateRequest();
  request->SetLoadFlags(net::LOAD_BYPASS_CACHE);

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(io_data()->ShouldAllowPreview(*request, PreviewsType::OFFLINE));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.Offline",
      static_cast<int>(PreviewsEligibilityReason::RELOAD_DISALLOWED), 1);
}

TEST_F(PreviewsIODataTest, TestAllowOffline) {
  InitializeUIService();

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(
      io_data()->ShouldAllowPreview(*CreateRequest(), PreviewsType::OFFLINE));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.Offline",
      static_cast<int>(PreviewsEligibilityReason::ALLOWED), 1);
}

TEST_F(PreviewsIODataTest, ClientLoFiDisallowedByDefault) {
  InitializeUIService();

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(io_data()->ShouldAllowPreviewAtECT(
      *CreateRequest(), PreviewsType::LOFI,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial()));
  histogram_tester.ExpectTotalCount("Previews.EligibilityReason.LoFi", 0);
}

TEST_F(PreviewsIODataTest, ClientLoFiDisallowedWhenFieldTrialDisabled) {
  InitializeUIService();
  CreateFieldTrialWithParams("PreviewsClientLoFi", "Disabled", {});

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(io_data()->ShouldAllowPreviewAtECT(
      *CreateRequest(), PreviewsType::LOFI,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial()));
  histogram_tester.ExpectTotalCount("Previews.EligibilityReason.LoFi", 0);
  variations::testing::ClearAllVariationParams();
}

TEST_F(PreviewsIODataTest, ClientLoFiDisallowedWhenNetworkQualityUnavailable) {
  InitializeUIService();
  CreateFieldTrialWithParams("PreviewsClientLoFi", "Enabled", {});

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(io_data()->ShouldAllowPreviewAtECT(
      *CreateRequest(), PreviewsType::LOFI,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial()));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.LoFi",
      static_cast<int>(PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE),
      1);
  variations::testing::ClearAllVariationParams();
}

TEST_F(PreviewsIODataTest, ClientLoFiDisallowedWhenNetworkFast) {
  InitializeUIService();
  CreateFieldTrialWithParams("PreviewsClientLoFi", "Enabled",
                             {{"max_allowed_effective_connection_type", "2G"}});

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_3G);

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(io_data()->ShouldAllowPreviewAtECT(
      *CreateRequest(), PreviewsType::LOFI,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial()));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.LoFi",
      static_cast<int>(PreviewsEligibilityReason::NETWORK_NOT_SLOW), 1);
  variations::testing::ClearAllVariationParams();
}

TEST_F(PreviewsIODataTest, ClientLoFiAllowed) {
  InitializeUIService();
  CreateFieldTrialWithParams("PreviewsClientLoFi", "Enabled",
                             {{"max_allowed_effective_connection_type", "2G"}});

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(io_data()->ShouldAllowPreviewAtECT(
      *CreateRequest(), PreviewsType::LOFI,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial()));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.LoFi",
      static_cast<int>(PreviewsEligibilityReason::ALLOWED), 1);
  variations::testing::ClearAllVariationParams();
}

TEST_F(PreviewsIODataTest, MissingHostDisallowed) {
  InitializeUIService();
  CreateFieldTrialWithParams("PreviewsClientLoFi", "Enabled",
                             {{"max_allowed_effective_connection_type", "2G"}});

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  EXPECT_FALSE(io_data()->ShouldAllowPreviewAtECT(
      *CreateRequestWithURL(GURL("file:///sdcard")), PreviewsType::LOFI,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial()));
  variations::testing::ClearAllVariationParams();
}

TEST_F(PreviewsIODataTest, ClientLoFiAllowedOnReload) {
  InitializeUIService();
  CreateFieldTrialWithParams("PreviewsClientLoFi", "Enabled",
                             {{"max_allowed_effective_connection_type", "2G"}});

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  std::unique_ptr<net::URLRequest> request = CreateRequest();
  request->SetLoadFlags(net::LOAD_BYPASS_CACHE);

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(io_data()->ShouldAllowPreviewAtECT(
      *request, PreviewsType::LOFI,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial()));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.LoFi",
      static_cast<int>(PreviewsEligibilityReason::ALLOWED), 1);
  variations::testing::ClearAllVariationParams();
}

TEST_F(PreviewsIODataTest, ClientLoFiObeysHostBlackListFromServer) {
  InitializeUIService();
  CreateFieldTrialWithParams("PreviewsClientLoFi", "Enabled",
                             {{"max_allowed_effective_connection_type", "2G"},
                              {"short_host_blacklist", "foo.com, ,bar.net "}});

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  const struct {
    const char* url;
    bool expected_client_lofi_allowed;
  } tests[] = {
      {"http://example.com", true},      {"http://foo.com", false},
      {"https://foo.com", false},        {"http://www.foo.com", true},
      {"http://m.foo.com", true},        {"http://foo.net", true},
      {"http://foo.com/example", false}, {"http://bar.net", false},
      {"http://bar.net.tld", true},
  };

  for (const auto& test : tests) {
    base::HistogramTester histogram_tester;

    std::unique_ptr<net::URLRequest> request =
        context()->CreateRequest(GURL(test.url), net::DEFAULT_PRIORITY, nullptr,
                                 TRAFFIC_ANNOTATION_FOR_TESTS);

    EXPECT_EQ(test.expected_client_lofi_allowed,
              io_data()->ShouldAllowPreviewAtECT(
                  *request, PreviewsType::LOFI,
                  params::EffectiveConnectionTypeThresholdForClientLoFi(),
                  params::GetBlackListedHostsForClientLoFiFieldTrial()));

    histogram_tester.ExpectUniqueSample(
        "Previews.EligibilityReason.LoFi",
        static_cast<int>(
            test.expected_client_lofi_allowed
                ? PreviewsEligibilityReason::ALLOWED
                : PreviewsEligibilityReason::HOST_BLACKLISTED_BY_SERVER),
        1);
  }
  variations::testing::ClearAllVariationParams();
}

TEST_F(PreviewsIODataTest, NoScriptDisallowedByDefault) {
  InitializeUIService();

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(io_data()->ShouldAllowPreviewAtECT(
      *CreateRequest(), PreviewsType::NOSCRIPT,
      previews::params::GetECTThresholdForPreview(
          previews::PreviewsType::NOSCRIPT),
      std::vector<std::string>()));
  histogram_tester.ExpectTotalCount("Previews.EligibilityReason.NoScript", 0);
}

TEST_F(PreviewsIODataTest, NoScriptAllowedByFeature) {
  InitializeUIService();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kNoScriptPreviews);

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(io_data()->ShouldAllowPreviewAtECT(
      *CreateHttpsRequest(), PreviewsType::NOSCRIPT,
      previews::params::GetECTThresholdForPreview(
          previews::PreviewsType::NOSCRIPT),
      std::vector<std::string>()));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.NoScript",
      static_cast<int>(
          PreviewsEligibilityReason::ALLOWED_WITHOUT_OPTIMIZATION_HINTS),
      1);
}

TEST_F(PreviewsIODataTest, NoScriptAllowedByFeatureWithWhitelist) {
  InitializeUIService();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kOptimizationHints}, {});

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  base::HistogramTester histogram_tester;

  // First verify no preview for non-whitelisted url.
  EXPECT_FALSE(io_data()->ShouldAllowPreviewAtECT(
      *CreateHttpsRequest(), PreviewsType::NOSCRIPT,
      previews::params::GetECTThresholdForPreview(
          previews::PreviewsType::NOSCRIPT),
      std::vector<std::string>()));

  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.NoScript",
      static_cast<int>(
          PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER),
      1);

  // Now verify preview for whitelisted url.
  EXPECT_TRUE(io_data()->ShouldAllowPreviewAtECT(
      *CreateRequestWithURL(GURL("https://whitelisted.example.com")),
      PreviewsType::NOSCRIPT,
      previews::params::GetECTThresholdForPreview(
          previews::PreviewsType::NOSCRIPT),
      std::vector<std::string>()));

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.NoScript",
      static_cast<int>(PreviewsEligibilityReason::ALLOWED), 1);
}

TEST_F(PreviewsIODataTest, LogPreviewNavigationPassInCorrectParams) {
  InitializeUIService();
  GURL url("http://www.url_a.com/url_a");
  bool opt_out = true;
  PreviewsType type = PreviewsType::OFFLINE;
  base::Time time = base::Time::Now();

  io_data()->LogPreviewNavigation(url, opt_out, type, time);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(ui_service()->navigation_urls(), ::testing::ElementsAre(url));
  EXPECT_THAT(ui_service()->navigation_opt_outs(),
              ::testing::ElementsAre(opt_out));
  EXPECT_THAT(ui_service()->navigation_types(), ::testing::ElementsAre(type));
  EXPECT_THAT(ui_service()->navigation_times(), ::testing::ElementsAre(time));
}

TEST_F(PreviewsIODataTest, LogPreviewDecisionMadePassInCorrectParams) {
  InitializeUIService();
  PreviewsEligibilityReason reason(
      PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE);
  GURL url("http://www.url_a.com/url_a");
  base::Time time = base::Time::Now();
  PreviewsType type = PreviewsType::OFFLINE;

  io_data()->LogPreviewDecisionMade(reason, url, time, type);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(ui_service()->decision_reasons(), ::testing::ElementsAre(reason));
  EXPECT_THAT(ui_service()->decision_urls(), ::testing::ElementsAre(url));
  EXPECT_THAT(ui_service()->decision_types(), ::testing::ElementsAre(type));
  EXPECT_THAT(ui_service()->decision_times(), ::testing::ElementsAre(time));
}

TEST_F(PreviewsIODataTest, BlacklistNotAvailableLogDecisionMade) {
  InitializeUIService();
  CreateFieldTrialWithParams("PreviewsClientLoFi", "Enabled", {});
  auto expected_reason = PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE;
  auto expected_type = PreviewsType::LOFI;

  std::unique_ptr<TestPreviewsBlackList> blacklist =
      base::MakeUnique<TestPreviewsBlackList>(expected_reason, io_data());
  io_data()->InjectTestBlacklist(std::move(blacklist));

  io_data()->ShouldAllowPreviewAtECT(*CreateRequest(), expected_type,
                                     net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
                                     {});
  base::RunLoop().RunUntilIdle();
  // Testing correct log method is called.
  EXPECT_THAT(ui_service()->decision_reasons(),
              ::testing::Contains(expected_reason));
  EXPECT_THAT(ui_service()->decision_types(),
              ::testing::Contains(expected_type));
}

TEST_F(PreviewsIODataTest, BlacklistStatusesLogDecisionMadeDefault) {
  InitializeUIService();
  CreateFieldTrialWithParams("PreviewsClientLoFi", "Enabled", {});

  PreviewsEligibilityReason expected_reasons[] = {
      PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
      PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
      PreviewsEligibilityReason::USER_BLACKLISTED,
      PreviewsEligibilityReason::HOST_BLACKLISTED,
  };

  auto expected_type = PreviewsType::LOFI;

  for (auto expected_reason : expected_reasons) {
    std::unique_ptr<TestPreviewsBlackList> blacklist =
        base::MakeUnique<TestPreviewsBlackList>(expected_reason, io_data());
    io_data()->InjectTestBlacklist(std::move(blacklist));

    io_data()->ShouldAllowPreviewAtECT(*CreateRequest(), expected_type,
                                       net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
                                       {});
    base::RunLoop().RunUntilIdle();
    // Testing correct log method is called.
    EXPECT_THAT(ui_service()->decision_reasons(),
                ::testing::Contains(expected_reason));
    EXPECT_THAT(ui_service()->decision_types(),
                ::testing::Contains(expected_type));
  }
}

TEST_F(PreviewsIODataTest, BlacklistStatusesLogDecisionMadeIgnore) {
  InitializeUIService();
  CreateFieldTrialWithParams("PreviewsClientLoFi", "Enabled", {});
  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_2G);
  auto expected_reason = PreviewsEligibilityReason::ALLOWED;
  auto expected_type = PreviewsType::LOFI;

  PreviewsEligibilityReason blacklist_decisions[] = {
      PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
      PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
      PreviewsEligibilityReason::USER_BLACKLISTED,
      PreviewsEligibilityReason::HOST_BLACKLISTED,
  };

  io_data()->SetIgnorePreviewsBlacklistDecision(true /* ignored */);

  for (auto blacklist_decision : blacklist_decisions) {
    std::unique_ptr<TestPreviewsBlackList> blacklist =
        base::MakeUnique<TestPreviewsBlackList>(blacklist_decision, io_data());
    io_data()->InjectTestBlacklist(std::move(blacklist));

    io_data()->ShouldAllowPreviewAtECT(
        *CreateRequest(), expected_type,
        params::EffectiveConnectionTypeThresholdForClientLoFi(),
        params::GetBlackListedHostsForClientLoFiFieldTrial());

    base::RunLoop().RunUntilIdle();
    // Testing correct log method is called.
    EXPECT_THAT(ui_service()->decision_reasons(),
                ::testing::Contains(expected_reason));
    EXPECT_THAT(ui_service()->decision_types(),
                ::testing::Contains(expected_type));
  }
}

TEST_F(PreviewsIODataTest, NetworkQualityNotAvailableCallsLogDecisionMade) {
  InitializeUIService();
  CreateFieldTrialWithParams("PreviewsClientLoFi", "Enabled", {});
  std::unique_ptr<TestPreviewsBlackList> blacklist =
      base::MakeUnique<TestPreviewsBlackList>(
          PreviewsEligibilityReason::ALLOWED, io_data());
  io_data()->InjectTestBlacklist(std::move(blacklist));

  auto expected_reason = PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE;
  auto expected_type = PreviewsType::LOFI;

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);

  io_data()->ShouldAllowPreviewAtECT(
      *CreateRequest(), expected_type,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial());

  base::RunLoop().RunUntilIdle();
  // Testing correct log method is called.
  EXPECT_THAT(ui_service()->decision_reasons(),
              ::testing::Contains(expected_reason));
  EXPECT_THAT(ui_service()->decision_types(),
              ::testing::Contains(expected_type));
}

TEST_F(PreviewsIODataTest, NetworkNotSlowLogDecisionMade) {
  InitializeUIService();
  CreateFieldTrialWithParams("PreviewsClientLoFi", "Enabled", {});
  std::unique_ptr<TestPreviewsBlackList> blacklist =
      base::MakeUnique<TestPreviewsBlackList>(
          PreviewsEligibilityReason::ALLOWED, io_data());
  io_data()->InjectTestBlacklist(std::move(blacklist));

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_4G);

  auto expected_reason = PreviewsEligibilityReason::NETWORK_NOT_SLOW;
  auto expected_type = PreviewsType::LOFI;

  io_data()->ShouldAllowPreviewAtECT(
      *CreateRequest(), expected_type,
      net::EFFECTIVE_CONNECTION_TYPE_2G /* threshold */, {});
  base::RunLoop().RunUntilIdle();
  // Testing correct log method is called.
  EXPECT_THAT(ui_service()->decision_reasons(),
              ::testing::Contains(expected_reason));
  EXPECT_THAT(ui_service()->decision_types(),
              ::testing::Contains(expected_type));
}

TEST_F(PreviewsIODataTest, HostBlacklistedLogDecisionMade) {
  InitializeUIService();
  CreateFieldTrialWithParams("PreviewsClientLoFi", "Enabled",
                             {{"short_host_blacklist", "example.com"}});
  std::unique_ptr<TestPreviewsBlackList> blacklist =
      base::MakeUnique<TestPreviewsBlackList>(
          PreviewsEligibilityReason::ALLOWED, io_data());
  io_data()->InjectTestBlacklist(std::move(blacklist));

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  auto expected_reason = PreviewsEligibilityReason::HOST_BLACKLISTED_BY_SERVER;
  auto expected_type = PreviewsType::LOFI;

  io_data()->ShouldAllowPreviewAtECT(
      *CreateRequest(), expected_type,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial());
  base::RunLoop().RunUntilIdle();

  // Testing correct log method is called.
  EXPECT_THAT(ui_service()->decision_reasons(),
              ::testing::Contains(expected_reason));
  EXPECT_THAT(ui_service()->decision_types(),
              ::testing::Contains(expected_type));
}

TEST_F(PreviewsIODataTest, ReloadDisallowedLogDecisionMade) {
  InitializeUIService();
  std::unique_ptr<TestPreviewsBlackList> blacklist =
      base::MakeUnique<TestPreviewsBlackList>(
          PreviewsEligibilityReason::ALLOWED, io_data());
  io_data()->InjectTestBlacklist(std::move(blacklist));

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_2G);
  std::unique_ptr<net::URLRequest> request = CreateRequest();
  request->SetLoadFlags(net::LOAD_BYPASS_CACHE);

  auto expected_reason = PreviewsEligibilityReason::RELOAD_DISALLOWED;
  auto expected_type = PreviewsType::OFFLINE;

  io_data()->ShouldAllowPreviewAtECT(
      *request, expected_type,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial());
  base::RunLoop().RunUntilIdle();

  // Testing correct log method is called.
  EXPECT_THAT(ui_service()->decision_reasons(),
              ::testing::Contains(expected_reason));
  EXPECT_THAT(ui_service()->decision_types(),
              ::testing::Contains(expected_type));
}

TEST_F(PreviewsIODataTest, AllowPreviewsOnECTLogDecisionMade) {
  InitializeUIService();
  CreateFieldTrialWithParams("PreviewsClientLoFi", "Enabled", {});

  std::unique_ptr<TestPreviewsBlackList> blacklist =
      base::MakeUnique<TestPreviewsBlackList>(
          PreviewsEligibilityReason::ALLOWED, io_data());

  io_data()->InjectTestBlacklist(std::move(blacklist));

  network_quality_estimator()->set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  auto expected_reason = PreviewsEligibilityReason::ALLOWED;
  auto expected_type = PreviewsType::LOFI;

  io_data()->ShouldAllowPreviewAtECT(
      *CreateRequest(), expected_type,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial());
  base::RunLoop().RunUntilIdle();

  // Testing correct log method is called.
  EXPECT_THAT(ui_service()->decision_reasons(),
              ::testing::Contains(expected_reason));
  EXPECT_THAT(ui_service()->decision_types(),
              ::testing::Contains(expected_type));
}

TEST_F(PreviewsIODataTest, OnNewBlacklistedHostCallsUIMethodCorrectly) {
  InitializeUIService();
  std::string expected_host = "example.com";
  base::Time expected_time = base::Time::Now();
  io_data()->OnNewBlacklistedHost(expected_host, expected_time);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(expected_host, ui_service()->host_blacklisted());
  EXPECT_EQ(expected_time, ui_service()->host_blacklisted_time());
}

TEST_F(PreviewsIODataTest, OnUserBlacklistedCallsUIMethodCorrectly) {
  InitializeUIService();
  io_data()->OnUserBlacklistedStatusChange(true /* blacklisted */);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ui_service()->user_blacklisted());

  io_data()->OnUserBlacklistedStatusChange(false /* blacklisted */);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(ui_service()->user_blacklisted());
}

TEST_F(PreviewsIODataTest, OnBlacklistClearedCallsUIMethodCorrectly) {
  InitializeUIService();
  base::Time expected_time = base::Time::Now();
  io_data()->OnBlacklistCleared(expected_time);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(expected_time, ui_service()->blacklist_cleared_time());
}

TEST_F(PreviewsIODataTest,
       OnIgnoreBlacklistDecisionStatusChangedCalledCorrect) {
  InitializeUIService();
  io_data()->SetIgnorePreviewsBlacklistDecision(true /* ignored */);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ui_service()->blacklist_ignored());

  io_data()->SetIgnorePreviewsBlacklistDecision(false /* ignored */);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ui_service()->blacklist_ignored());
}

TEST_F(PreviewsIODataTest, GeneratePageIdMakesUniqueNonZero) {
  InitializeUIService();
  std::unordered_set<uint64_t> page_id_set;
  size_t number_of_generated_ids = 10;
  for (size_t i = 0; i < number_of_generated_ids; i++) {
    page_id_set.insert(io_data()->GeneratePageId());
  }
  EXPECT_EQ(number_of_generated_ids, page_id_set.size());
  EXPECT_EQ(page_id_set.end(), page_id_set.find(0u));
}

}  // namespace

}  // namespace previews
