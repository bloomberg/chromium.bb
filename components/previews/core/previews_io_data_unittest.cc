// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_io_data.h"

#include <initializer_list>
#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/previews/core/previews_black_list.h"
#include "components/previews/core/previews_black_list_item.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_logger.h"
#include "components/previews/core/previews_opt_out_store.h"
#include "components/previews/core/previews_ui_service.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/load_flags.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
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
    case PreviewsType::NOSCRIPT:
      return params::IsOfflinePreviewsEnabled();
    case PreviewsType::LOFI:
      return params::IsClientLoFiEnabled();
    case PreviewsType::NONE:
    case PreviewsType::LAST:
      break;
  }
  NOTREACHED();
  return false;
}

// Stub class of PreviewsUIService to test logging functionalities in
// PreviewsIOData.
class TestPreviewsUIService : public PreviewsUIService {
 public:
  TestPreviewsUIService(
      PreviewsIOData* previews_io_data,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      std::unique_ptr<PreviewsOptOutStore> previews_opt_out_store,
      const PreviewsIsEnabledCallback& is_enabled_callback,
      std::unique_ptr<PreviewsLogger> logger)
      : PreviewsUIService(previews_io_data,
                          io_task_runner,
                          std::move(previews_opt_out_store),
                          is_enabled_callback,
                          std::move(logger)),
        type_(PreviewsType::NONE) {}

  // Return the passed in url.
  GURL url() const { return url_; }

  // Return the passed in opt_out.
  bool opt_out() const { return opt_out_; }

  // Return the passed in type.
  PreviewsType type() const { return type_; }

  // Return the passed in time.
  base::Time time() const { return time_; }

 private:
  // PreviewsUIService:
  void LogPreviewNavigation(const GURL& url,
                            PreviewsType type,
                            bool opt_out,
                            base::Time time) override {
    url_ = url;
    opt_out_ = opt_out;
    type_ = type;
    time_ = base::Time(time);
  }

  GURL url_;
  bool opt_out_;
  PreviewsType type_;
  base::Time time_;
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
        io_data_(loop_.task_runner(), loop_.task_runner()),
        context_(true) {
    context_.set_network_quality_estimator(&network_quality_estimator_);
    context_.Init();

    network_quality_estimator_.set_effective_connection_type(
        net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  }

  ~PreviewsIODataTest() override {
    variations::testing::ClearAllVariationParams();
  }

  void InitializeUIServiceWithoutWaitingForBlackList() {
    ui_service_.reset(
        new TestPreviewsUIService(&io_data_, loop_.task_runner(),
                                  base::MakeUnique<TestPreviewsOptOutStore>(),
                                  base::Bind(&IsPreviewFieldTrialEnabled),
                                  base::MakeUnique<PreviewsLogger>()));
  }

  void InitializeUIService() {
    InitializeUIServiceWithoutWaitingForBlackList();
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<net::URLRequest> CreateRequest() const {
    return CreateRequestWithURL(GURL("http://example.com"));
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

 protected:
  base::MessageLoopForIO loop_;

 private:
  base::FieldTrialList field_trial_list_;
  TestPreviewsIOData io_data_;
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

TEST_F(PreviewsIODataTest, LogPreviewNavigationPassInCorrectParams) {
  InitializeUIService();
  GURL url("http://www.url_a.com/url_a");
  bool opt_out = true;
  PreviewsType type = PreviewsType::OFFLINE;
  base::Time time = base::Time::Now();

  io_data()->LogPreviewNavigation(url, opt_out, type, time);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(url, ui_service()->url());
  EXPECT_EQ(opt_out, ui_service()->opt_out());
  EXPECT_EQ(type, ui_service()->type());
  EXPECT_EQ(time, ui_service()->time());
}

}  // namespace

}  // namespace previews
