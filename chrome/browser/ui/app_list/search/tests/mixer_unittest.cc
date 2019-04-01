// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/mixer.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/files/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "testing/gtest/include/gtest/gtest.h"

class FakeAppListModelUpdater;

namespace app_list {
namespace test {

using ResultType = ash::SearchResultType;

// Maximum number of results to show in each mixer group.
const size_t kMaxAppsGroupResults = 4;
const size_t kMaxOmniboxResults = 4;
const size_t kMaxWebstoreResults = 2;

const bool kEphemeralUser = false;

class TestSearchResult : public ChromeSearchResult {
 public:
  TestSearchResult(const std::string& id, double relevance)
      : instance_id_(instantiation_count++) {
    set_id(id);
    SetTitle(base::UTF8ToUTF16(id));
    set_relevance(relevance);
  }
  ~TestSearchResult() override {}

  // ChromeSearchResult overrides:
  void Open(int event_flags) override {}
  void InvokeAction(int action_index, int event_flags) override {}
  SearchResultType GetSearchResultType() const override {
    return app_list::SEARCH_RESULT_TYPE_BOUNDARY;
  }

  // For reference equality testing. (Addresses cannot be used to test reference
  // equality because it is possible that an object will be allocated at the
  // same address as a previously deleted one.)
  static int GetInstanceId(ChromeSearchResult* result) {
    return static_cast<const TestSearchResult*>(result)->instance_id_;
  }

 private:
  static int instantiation_count;

  int instance_id_;

  DISALLOW_COPY_AND_ASSIGN(TestSearchResult);
};
int TestSearchResult::instantiation_count = 0;

class TestSearchProvider : public SearchProvider {
 public:
  TestSearchProvider(const std::string& prefix, ResultType result_type)
      : prefix_(prefix),
        count_(0),
        bad_relevance_range_(false),
        small_relevance_range_(false),
        display_type_(ash::SearchResultDisplayType::kList),
        result_type_(result_type) {}
  ~TestSearchProvider() override {}

  // SearchProvider overrides:
  void Start(const base::string16& query) override {
    ClearResults();
    for (size_t i = 0; i < count_; ++i) {
      const std::string id =
          base::StringPrintf("%s%d", prefix_.c_str(), static_cast<int>(i));
      double relevance = 1.0 - i / 10.0;
      // If bad_relevance_range_, change the relevances to give results outside
      // of the canonical [0.0, 1.0] range.
      if (bad_relevance_range_)
        relevance = 10.0 - i * 10;
      // If |small_relevance_range_|, keep the relevances in the same order, but
      // make the differences very small: 0.5, 0.499, 0.498, ...
      if (small_relevance_range_)
        relevance = 0.5 - i / 100.0;
      TestSearchResult* result = new TestSearchResult(id, relevance);
      result->SetDisplayType(display_type_);
      result->SetResultType(result_type_);
      Add(std::unique_ptr<ChromeSearchResult>(result));
    }
  }

  void set_prefix(const std::string& prefix) { prefix_ = prefix; }
  void SetDisplayType(ChromeSearchResult::DisplayType display_type) {
    display_type_ = display_type;
  }
  void set_count(size_t count) { count_ = count; }
  void set_bad_relevance_range() { bad_relevance_range_ = true; }
  void set_small_relevance_range() { small_relevance_range_ = true; }

 private:
  std::string prefix_;
  size_t count_;
  bool bad_relevance_range_;
  bool small_relevance_range_;
  ChromeSearchResult::DisplayType display_type_;
  ResultType result_type_;

  DISALLOW_COPY_AND_ASSIGN(TestSearchProvider);
};

class MixerTest : public testing::Test {
 public:
  MixerTest() {}
  ~MixerTest() override {}

  // testing::Test overrides:
  void SetUp() override {
    model_updater_ = std::make_unique<FakeAppListModelUpdater>();

    providers_.push_back(
        std::make_unique<TestSearchProvider>("app", ResultType::kInternalApp));
    providers_.push_back(
        std::make_unique<TestSearchProvider>("omnibox", ResultType::kOmnibox));
    providers_.push_back(std::make_unique<TestSearchProvider>(
        "webstore", ResultType::kWebStoreApp));
  }

  void CreateMixer(bool use_adaptive_ranker,
                   const std::map<std::string, std::string>& params = {}) {
    if (use_adaptive_ranker) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          app_list_features::kEnableAdaptiveResultRanker, params);
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {app_list_features::kEnableAdaptiveResultRanker});
    }

    mixer_ = std::make_unique<Mixer>(model_updater_.get());

    if (use_adaptive_ranker) {
      ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

      RecurrenceRankerConfigProto ranker_config;
      ranker_config.set_min_seconds_between_saves(240u);
      auto* predictor = ranker_config.mutable_zero_state_frecency_predictor();
      predictor->set_target_limit(200u);
      predictor->set_decay_coeff(0.8f);
      auto* fallback = ranker_config.mutable_fallback_predictor();
      fallback->set_target_limit(200u);
      fallback->set_decay_coeff(0.8f);

      std::unique_ptr<RecurrenceRanker> ranker =
          std::make_unique<RecurrenceRanker>(
              temp_dir_.GetPath().AppendASCII("ranker_model.proto"),
              ranker_config, kEphemeralUser);
      Wait();
      mixer_->SetRecurrenceRanker(std::move(ranker));
    }

    // TODO(warx): when fullscreen app list is default enabled, modify this test
    // to test answer card/apps group having relevance boost.
    size_t apps_group_id = mixer_->AddGroup(kMaxAppsGroupResults, 1.0, 0.0);
    size_t omnibox_group_id = mixer_->AddGroup(kMaxOmniboxResults, 1.0, 0.0);
    size_t webstore_group_id = mixer_->AddGroup(kMaxWebstoreResults, 0.5, 0.0);

    mixer_->AddProviderToGroup(apps_group_id, providers_[0].get());
    mixer_->AddProviderToGroup(omnibox_group_id, providers_[1].get());
    mixer_->AddProviderToGroup(webstore_group_id, providers_[2].get());
  }

  void RunQuery() {
    const base::string16 query;

    for (size_t i = 0; i < providers_.size(); ++i)
      providers_[i]->Start(query);

    mixer_->MixAndPublish(AppListConfig::instance().max_search_results());
  }

  std::string GetResults() const {
    auto& results = model_updater_->search_results();
    std::string result;
    for (size_t i = 0; i < results.size(); ++i) {
      if (!result.empty())
        result += ',';

      result += base::UTF16ToUTF8(results[i]->title());
    }

    return result;
  }

  void Train(const std::string& id, const RankingItemType& type) {
    mixer_->Train(id, type);
  }

  void Wait() { scoped_task_environment_.RunUntilIdle(); }

  Mixer* mixer() { return mixer_.get(); }
  TestSearchProvider* app_provider() { return providers_[0].get(); }
  TestSearchProvider* omnibox_provider() { return providers_[1].get(); }
  TestSearchProvider* webstore_provider() { return providers_[2].get(); }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<Mixer> mixer_;
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;

  std::vector<std::unique_ptr<TestSearchProvider>> providers_;

  DISALLOW_COPY_AND_ASSIGN(MixerTest);
};

TEST_F(MixerTest, Basic) {
  // Create mixer without adaptive ranker.
  CreateMixer(false);

  // Note: Some cases in |expected| have vastly more results than others, due to
  // the "at least 6" mechanism. If it gets at least 6 results from all
  // providers, it stops at 6. If not, it fetches potentially many more results
  // from all providers. Not ideal, but currently by design.
  struct TestCase {
    const size_t app_results;
    const size_t omnibox_results;
    const size_t webstore_results;
    const char* expected;
  } kTestCases[] = {
      {0, 0, 0, ""},
      {10, 0, 0, "app0,app1,app2,app3,app4,app5,app6,app7,app8,app9"},
      {0, 0, 10,
       "webstore0,webstore1,webstore2,webstore3,webstore4,webstore5,webstore6,"
       "webstore7,webstore8,webstore9"},
      {4, 6, 0, "app0,omnibox0,app1,omnibox1,app2,omnibox2,app3,omnibox3"},
      {4, 6, 2,
       "app0,omnibox0,app1,omnibox1,app2,omnibox2,app3,omnibox3,webstore0,"
       "webstore1"},
      {10, 10, 10,
       "app0,omnibox0,app1,omnibox1,app2,omnibox2,app3,omnibox3,webstore0,"
       "webstore1"},
      {0, 10, 0,
       "omnibox0,omnibox1,omnibox2,omnibox3,omnibox4,omnibox5,omnibox6,"
       "omnibox7,omnibox8,omnibox9"},
      {0, 10, 1,
       "omnibox0,omnibox1,omnibox2,omnibox3,webstore0,omnibox4,omnibox5,"
       "omnibox6,omnibox7,omnibox8,omnibox9"},
      {0, 10, 2, "omnibox0,omnibox1,omnibox2,omnibox3,webstore0,webstore1"},
      {1, 10, 0,
       "app0,omnibox0,omnibox1,omnibox2,omnibox3,omnibox4,omnibox5,omnibox6,"
       "omnibox7,omnibox8,omnibox9"},
      {2, 10, 0, "app0,omnibox0,app1,omnibox1,omnibox2,omnibox3"},
      {2, 10, 1, "app0,omnibox0,app1,omnibox1,omnibox2,omnibox3,webstore0"},
      {2, 10, 2,
       "app0,omnibox0,app1,omnibox1,omnibox2,omnibox3,webstore0,webstore1"},
      {2, 0, 2, "app0,app1,webstore0,webstore1"},
      {0, 0, 0, ""},
  };

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    app_provider()->set_count(kTestCases[i].app_results);
    omnibox_provider()->set_count(kTestCases[i].omnibox_results);
    webstore_provider()->set_count(kTestCases[i].webstore_results);
    RunQuery();

    EXPECT_EQ(kTestCases[i].expected, GetResults()) << "Case " << i;
  }
}

TEST_F(MixerTest, RemoveDuplicates) {
  // Create mixer without adaptive ranker.
  CreateMixer(false);

  const std::string dup = "dup";

  // This gives "dup0,dup1,dup2".
  app_provider()->set_prefix(dup);
  app_provider()->set_count(3);

  // This gives "dup0,dup1".
  omnibox_provider()->set_prefix(dup);
  omnibox_provider()->set_count(2);

  // This gives "dup0".
  webstore_provider()->set_prefix(dup);
  webstore_provider()->set_count(1);

  RunQuery();

  // Only three results with unique id are kept.
  EXPECT_EQ("dup0,dup1,dup2", GetResults());
}

TEST_F(MixerTest, RankerIsDisabledWithFlag) {
  CreateMixer(false);

  for (int i = 0; i < 20; ++i)
    Train("omnibox2", RankingItemType::kOmniboxGeneric);

  app_provider()->set_count(4);
  app_provider()->set_small_relevance_range();
  omnibox_provider()->set_count(4);
  omnibox_provider()->set_small_relevance_range();
  RunQuery();

  // Expect training calls to have not affected rankings.
  EXPECT_EQ(GetResults(),
            "app0,omnibox0,app1,omnibox1,app2,omnibox2,app3,omnibox3");
}

TEST_F(MixerTest, RankerImprovesScores) {
  CreateMixer(true, {{"boost_coefficient", "10.0"}});

  for (int i = 0; i < 20; ++i)
    Train("omnibox2", RankingItemType::kOmniboxGeneric);

  app_provider()->set_count(4);
  app_provider()->set_small_relevance_range();
  omnibox_provider()->set_count(4);
  omnibox_provider()->set_small_relevance_range();
  RunQuery();

  // Omnibox results exist in the ranker and should be up-weighted to the top of
  // the list.
  EXPECT_EQ(GetResults(),
            "omnibox0,omnibox1,omnibox2,omnibox3,app0,app1,app2,app3");
}

}  // namespace test
}  // namespace app_list
