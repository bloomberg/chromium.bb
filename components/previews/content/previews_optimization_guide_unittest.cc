// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_optimization_guide.h"

#include <memory>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/previews/content/previews_top_host_provider.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/bloom_filter.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

namespace {
// A fake default page_id for testing.
const uint64_t kDefaultPageId = 123456;
}  // namespace

class TestOptimizationGuideService
    : public optimization_guide::OptimizationGuideService {
 public:
  explicit TestOptimizationGuideService(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
      : OptimizationGuideService(ui_task_runner),
        add_observer_called_(false),
        remove_observer_called_(false) {}

  void AddObserver(
      optimization_guide::OptimizationGuideServiceObserver* observer) override {
    add_observer_called_ = true;
  }

  void RemoveObserver(
      optimization_guide::OptimizationGuideServiceObserver* observer) override {
    remove_observer_called_ = true;
  }

  bool AddObserverCalled() { return add_observer_called_; }
  bool RemoveObserverCalled() { return remove_observer_called_; }

 private:
  bool add_observer_called_;
  bool remove_observer_called_;
};

// A test class implementation for unit testing previews_optimization_guide.
class TestPreviewsTopHostProvider : public PreviewsTopHostProvider {
 public:
  TestPreviewsTopHostProvider() {}
  ~TestPreviewsTopHostProvider() override {}

  std::vector<std::string> GetTopHosts(size_t max_sites) const override {
    return std::vector<std::string>();
  }
};

class PreviewsOptimizationGuideTest : public testing::Test {
 public:
  PreviewsOptimizationGuideTest() {}

  ~PreviewsOptimizationGuideTest() override {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    CreateServiceAndGuide();
  }

  // Delete |guide_| if it hasn't been deleted.
  void TearDown() override { ResetGuide(); }

  PreviewsOptimizationGuide* guide() { return guide_.get(); }

  TestOptimizationGuideService* optimization_guide_service() {
    return optimization_guide_service_.get();
  }

  void ProcessHints(const optimization_guide::proto::Configuration& config,
                    const std::string& version) {
    optimization_guide::HintsComponentInfo info(
        base::Version(version),
        temp_dir().Append(FILE_PATH_LITERAL("somefile.pb")));
    ASSERT_NO_FATAL_FAILURE(WriteConfigToFile(config, info.path));
    guide_->OnHintsComponentAvailable(info);
    RunUntilIdle();
  }

  void CreateServiceAndGuide() {
    if (guide_) {
      ResetGuide();
    }
    optimization_guide_service_ =
        std::make_unique<TestOptimizationGuideService>(
            scoped_task_environment_.GetMainThreadTaskRunner());
    guide_ = std::make_unique<PreviewsOptimizationGuide>(
        optimization_guide_service_.get(),
        scoped_task_environment_.GetMainThreadTaskRunner(), temp_dir(),
        previews_top_host_provider_.get());

    // Add observer is called after the HintCache is fully initialized,
    // indicating that the PreviewsOptimizationGuide is ready to process hints.
    while (!optimization_guide_service_->AddObserverCalled()) {
      RunUntilIdle();
    }
  }

  void ResetGuide() {
    guide_.reset();
    RunUntilIdle();
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

 protected:
  void RunUntilIdle() {
    scoped_task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  void DoExperimentFlagTest(base::Optional<std::string> experiment_name,
                            bool expect_enabled);

  // This is a helper function for initializing fixed number of ResourceLoading
  // hints.
  void InitializeFixedCountResourceLoadingHints();

  // This is a helper function for initializing fixed number of ResourceLoading
  // hints that contain an experiment
  void InitializeFixedCountResourceLoadingHintsWithTwoExperiments();

  // This is a helper function for initializing multiple ResourceLoading hints.
  // The generated hint proto contains hints for |key_count| keys.
  // |page_patterns_per_key| page patterns are specified per key.
  // For each page pattern, 2 resource loading hints are specified in the proto.
  void InitializeMultipleResourceLoadingHints(size_t key_count,
                                              size_t page_patterns_per_key);

  // This is a helper function for initializing with a LITE_PAGE_REDIRECT
  // server blacklist.
  void InitializeWithLitePageRedirectBlacklist();

  // This function guarantees that all of the asynchronous processing required
  // to load the specified hint has occurred prior to calling IsWhitelisted.
  // It accomplishes this by calling MaybeLoadOptimizationHints() and waiting
  // until OnLoadOptimizationHints runs before calling IsWhitelisted().
  bool MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      PreviewsUserData* previews_data,
      const GURL& url,
      PreviewsType type,
      net::EffectiveConnectionType* out_ect_threshold);

 private:
  void WriteConfigToFile(const optimization_guide::proto::Configuration& config,
                         const base::FilePath& filePath) {
    std::string serialized_config;
    ASSERT_TRUE(config.SerializeToString(&serialized_config));
    ASSERT_EQ(static_cast<int32_t>(serialized_config.length()),
              base::WriteFile(filePath, serialized_config.data(),
                              serialized_config.length()));
  }

  // Callback used to indicate that the asynchronous call to
  // MaybeLoadOptimizationHints() has completed its processing.
  void OnLoadOptimizationHints();

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<PreviewsOptimizationGuide> guide_;
  std::unique_ptr<TestOptimizationGuideService> optimization_guide_service_;
  std::unique_ptr<TestPreviewsTopHostProvider> previews_top_host_provider_;

  // Flag set when the OnLoadOptimizationHints callback runs. This indicates
  // that MaybeLoadOptimizationHints() has completed its processing.
  bool requested_hints_loaded_;

  GURL loaded_hints_document_gurl_;
  std::vector<std::string> loaded_hints_resource_patterns_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsOptimizationGuideTest);
};

void PreviewsOptimizationGuideTest::InitializeFixedCountResourceLoadingHints() {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);

  // Page hint for "/news/"
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("/news/");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_loading_hint1 =
      optimization1->add_resource_loading_hints();
  resource_loading_hint1->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_loading_hint1->set_resource_pattern("news_cruft.js");

  // Page hint for "football"
  optimization_guide::proto::PageHint* page_hint2 = hint1->add_page_hints();
  page_hint2->set_page_pattern("football");
  optimization_guide::proto::Optimization* optimization2 =
      page_hint2->add_whitelisted_optimizations();
  optimization2->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_loading_hint2 =
      optimization2->add_resource_loading_hints();
  resource_loading_hint2->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_loading_hint2->set_resource_pattern("football_cruft.js");

  optimization_guide::proto::ResourceLoadingHint* resource_loading_hint3 =
      optimization2->add_resource_loading_hints();
  resource_loading_hint3->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_loading_hint3->set_resource_pattern("barball_cruft.js");

  ProcessHints(config, "2.0.0");
}

void PreviewsOptimizationGuideTest::
    InitializeFixedCountResourceLoadingHintsWithTwoExperiments() {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);

  // Page hint for "/news/"
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("/news/");

  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_experiment_name("experiment_1");
  optimization1->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_loading_hint1 =
      optimization1->add_resource_loading_hints();
  resource_loading_hint1->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_loading_hint1->set_resource_pattern("news_cruft.js");

  optimization_guide::proto::Optimization* optimization2 =
      page_hint1->add_whitelisted_optimizations();
  optimization2->set_experiment_name("experiment_2");
  optimization2->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_loading_hint2 =
      optimization2->add_resource_loading_hints();
  resource_loading_hint2->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_loading_hint2->set_resource_pattern("football_cruft.js");

  ProcessHints(config, "2.0.0");
}

void PreviewsOptimizationGuideTest::InitializeMultipleResourceLoadingHints(
    size_t key_count,
    size_t page_patterns_per_key) {
  optimization_guide::proto::Configuration config;

  for (size_t key_index = 0; key_index < key_count; ++key_index) {
    optimization_guide::proto::Hint* hint = config.add_hints();
    hint->set_key("somedomain" + base::NumberToString(key_index) + ".org");
    hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);

    for (size_t page_pattern_index = 0;
         page_pattern_index < page_patterns_per_key; ++page_pattern_index) {
      // Page hint for "/news/"
      optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
      page_hint->set_page_pattern(
          "/news" + base::NumberToString(page_pattern_index) + "/");
      optimization_guide::proto::Optimization* optimization1 =
          page_hint->add_whitelisted_optimizations();
      optimization1->set_optimization_type(
          optimization_guide::proto::RESOURCE_LOADING);

      optimization_guide::proto::ResourceLoadingHint* resource_loading_hint_1 =
          optimization1->add_resource_loading_hints();
      resource_loading_hint_1->set_loading_optimization_type(
          optimization_guide::proto::LOADING_BLOCK_RESOURCE);
      resource_loading_hint_1->set_resource_pattern("news_cruft_1.js");

      optimization_guide::proto::ResourceLoadingHint* resource_loading_hint_2 =
          optimization1->add_resource_loading_hints();
      resource_loading_hint_2->set_loading_optimization_type(
          optimization_guide::proto::LOADING_BLOCK_RESOURCE);
      resource_loading_hint_2->set_resource_pattern("news_cruft_2.js");
    }
  }

  ProcessHints(config, "2.0.0");
}

void PreviewsOptimizationGuideTest::InitializeWithLitePageRedirectBlacklist() {
  previews::BloomFilter blacklist_bloom_filter(7, 511);
  blacklist_bloom_filter.Add("blacklisteddomain.com");
  blacklist_bloom_filter.Add("blacklistedsubdomain.maindomain.co.in");
  std::string blacklist_data((char*)&blacklist_bloom_filter.bytes()[0],
                             blacklist_bloom_filter.bytes().size());
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::OptimizationFilter* blacklist_proto =
      config.add_optimization_blacklists();
  blacklist_proto->set_optimization_type(
      optimization_guide::proto::LITE_PAGE_REDIRECT);
  std::unique_ptr<optimization_guide::proto::BloomFilter> bloom_filter_proto =
      std::make_unique<optimization_guide::proto::BloomFilter>();
  bloom_filter_proto->set_num_hash_functions(7);
  bloom_filter_proto->set_num_bits(511);
  bloom_filter_proto->set_data(blacklist_data);
  blacklist_proto->set_allocated_bloom_filter(bloom_filter_proto.release());
  ProcessHints(config, "2.0.0");
}

bool PreviewsOptimizationGuideTest::
    MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
        PreviewsUserData* previews_data,
        const GURL& url,
        PreviewsType type,
        net::EffectiveConnectionType* out_ect_threshold) {
  // Ensure that all asynchronous MaybeLoadOptimizationHints processing
  // finishes prior to calling IsWhitelisted. This is accomplished by waiting
  // for the OnLoadOptimizationHints callback to set |requested_hints_loaded_|
  // to true.
  requested_hints_loaded_ = false;
  if (guide()->MaybeLoadOptimizationHints(
          url, base::BindOnce(
                   &PreviewsOptimizationGuideTest::OnLoadOptimizationHints,
                   base::Unretained(this)))) {
    while (!requested_hints_loaded_) {
      RunUntilIdle();
    }
  }

  return guide()->IsWhitelisted(previews_data, url, type, out_ect_threshold);
}

void PreviewsOptimizationGuideTest::OnLoadOptimizationHints() {
  requested_hints_loaded_ = true;
}

TEST_F(PreviewsOptimizationGuideTest, IsWhitelistedWithoutHints) {
  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsForNoScriptPageHintsPopulatedCorrectly) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});

  // Configure somedomain.org with 2 page patterns, different ECT thresholds.
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("noscript_default_2g");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);
  optimization_guide::proto::PageHint* page_hint2 = hint1->add_page_hints();
  page_hint2->set_page_pattern("noscript_3g");
  page_hint2->set_max_ect_trigger(
      optimization_guide::proto::EffectiveConnectionType::
          EFFECTIVE_CONNECTION_TYPE_3G);
  optimization_guide::proto::Optimization* optimization2 =
      page_hint2->add_whitelisted_optimizations();
  optimization2->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  // Configure anypage.com with * page pattern.
  optimization_guide::proto::Hint* hint2 = config.add_hints();
  hint2->set_key("anypage.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint3 = hint2->add_page_hints();
  page_hint3->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization3 =
      page_hint3->add_whitelisted_optimizations();
  optimization3->set_optimization_type(optimization_guide::proto::NOSCRIPT);
  ProcessHints(config, "2.0.0");

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;

  // Verify page matches and ECT thresholds.
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://somedomain.org/noscript_default_2g"),
      PreviewsType::NOSCRIPT, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://somedomain.org/noscript_3g"),
      PreviewsType::NOSCRIPT, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_3G, ect_threshold);
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://somedomain.org/no_pattern_match"),
      PreviewsType::NOSCRIPT, &ect_threshold));

  // Verify * matches any page.
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://anypage.com/noscript_for_all"),
      PreviewsType::NOSCRIPT, &ect_threshold));
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://anypage.com/"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://anypage.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsWithValidCommandLineOverride) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("noscript_default_2g");
  optimization_guide::proto::Optimization* optimization =
      page_hint->add_whitelisted_optimizations();
  optimization->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  base::Base64Encode(encoded_config, &encoded_config);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kHintsProtoOverride, encoded_config);
  CreateServiceAndGuide();

  // Verify page matches and ECT thresholds.
  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;

  EXPECT_TRUE(guide()->GetHintsForTesting());
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://somedomain.org/noscript_default_2g"),
      PreviewsType::NOSCRIPT, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsWithValidCommandLineOverrideAndPreexistingData) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});

  InitializeFixedCountResourceLoadingHints();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("otherdomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("noscript_default_2g");
  optimization_guide::proto::Optimization* optimization =
      page_hint->add_whitelisted_optimizations();
  optimization->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  base::Base64Encode(encoded_config, &encoded_config);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kHintsProtoOverride, encoded_config);
  CreateServiceAndGuide();

  // Verify page matches and ECT thresholds.
  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;

  EXPECT_TRUE(guide()->GetHintsForTesting());
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://otherdomain.org/noscript_default_2g"),
      PreviewsType::NOSCRIPT, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);

  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsWithInvalidCommandLineOverride) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kHintsProtoOverride, "this-is-not-a-proto");
  CreateServiceAndGuide();

  EXPECT_FALSE(guide()->GetHintsForTesting());
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsWithPurgeHintCacheStoreCommandLineAndNoPreexistingData) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kPurgeHintCacheStore);
  CreateServiceAndGuide();

  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));

  InitializeFixedCountResourceLoadingHints();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsWithPurgeHintCacheStoreCommandLineAndPreexistingData) {
  InitializeFixedCountResourceLoadingHints();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kPurgeHintCacheStore);
  CreateServiceAndGuide();

  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));

  InitializeFixedCountResourceLoadingHints();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));
}

// Test when resource loading hints are enabled.
TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsForResourceLoadingHintsPopulatedCorrectly) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  // Add first hint.
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_hint1 =
      optimization1->add_resource_loading_hints();
  resource_hint1->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_hint1->set_resource_pattern("resource.js");

  // Add an additional optimization to first hint to verify that the applicable
  // optimizations are still whitelisted.
  optimization_guide::proto::PageHint* page_hint2 = hint1->add_page_hints();
  page_hint2->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization2 =
      page_hint2->add_whitelisted_optimizations();
  optimization2->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  // Add second hint.
  optimization_guide::proto::Hint* hint2 = config.add_hints();
  hint2->set_key("twitter.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint3 = hint2->add_page_hints();
  page_hint3->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization3 =
      page_hint3->add_whitelisted_optimizations();
  optimization3->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_hint2 =
      optimization3->add_resource_loading_hints();
  resource_hint2->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_hint2->set_resource_pattern("resource.js");

  ProcessHints(config, "2.0.0");

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  // Twitter and Facebook should be whitelisted but not Google.
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.twitter.com/example"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://google.com"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
}

// Test when both NoScript and resource loading hints are enabled.
TEST_F(
    PreviewsOptimizationGuideTest,
    ProcessHintsWhitelistForNoScriptAndResourceLoadingHintsPopulatedCorrectly) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});

  // Add first hint.
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  // Add additional optimization to second hint to verify that the applicable
  // optimizations are still whitelisted.
  optimization_guide::proto::Optimization* optimization2 =
      page_hint1->add_whitelisted_optimizations();
  optimization2->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_hint1 =
      optimization2->add_resource_loading_hints();
  resource_hint1->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_hint1->set_resource_pattern("resource.js");

  // Add second hint.
  optimization_guide::proto::Hint* hint2 = config.add_hints();
  hint2->set_key("twitter.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint2 = hint2->add_page_hints();
  page_hint2->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization3 =
      page_hint2->add_whitelisted_optimizations();
  optimization3->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_hint2 =
      optimization3->add_resource_loading_hints();
  resource_hint2->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_hint2->set_resource_pattern("resource.js");

  ProcessHints(config, "2.0.0");

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  // Twitter and Facebook should be whitelisted but not Google.
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com/example.html"),
      PreviewsType::NOSCRIPT, &ect_threshold));
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.twitter.com/example"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://google.com"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsForResourceLoadingHintsWithSlowPageTriggering) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  // Hint with 3G threshold.
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("3g.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  page_hint1->set_max_ect_trigger(
      optimization_guide::proto::EffectiveConnectionType::
          EFFECTIVE_CONNECTION_TYPE_3G);
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);

  // Hint with 4G threshold.
  optimization_guide::proto::Hint* hint2 = config.add_hints();
  hint2->set_key("4g.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint2 = hint2->add_page_hints();
  page_hint2->set_page_pattern("*");
  page_hint2->set_max_ect_trigger(
      optimization_guide::proto::EffectiveConnectionType::
          EFFECTIVE_CONNECTION_TYPE_4G);
  optimization_guide::proto::Optimization* optimization2 =
      page_hint2->add_whitelisted_optimizations();
  optimization2->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);

  // Hint with no threshold (default case).
  optimization_guide::proto::Hint* hint3 = config.add_hints();
  hint3->set_key("default2g.com");
  hint3->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint3 = hint3->add_page_hints();
  page_hint3->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization3 =
      page_hint3->add_whitelisted_optimizations();
  optimization3->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);

  ProcessHints(config, "2.0.0");

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://3g.com"), PreviewsType::RESOURCE_LOADING_HINTS,
      &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_3G, ect_threshold);
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://4g.com/example"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_4G, ect_threshold);
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://default2g.com"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);
}

// This is a helper function for testing the experiment flags on the config for
// the optimization guide. It creates a test config with a hint containing
// multiple optimizations. The optimization under test will be marked with an
// experiment name if one is provided in |experiment_name|. It will then be
// tested to see if it's enabled, the expectation found in |expect_enabled|.
void PreviewsOptimizationGuideTest::DoExperimentFlagTest(
    base::Optional<std::string> experiment_name,
    bool expect_enabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});

  // Create a hint with two optimizations. One may be marked experimental
  // depending on test configuration. The other is never marked experimental.
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  // NOSCRIPT is the optimization under test and may be marked experimental.
  if (experiment_name.has_value()) {
    optimization1->set_experiment_name(experiment_name.value());
  }
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  // RESOURCE_LOADING is not marked experimental.
  optimization_guide::proto::Optimization* optimization2 =
      page_hint1->add_whitelisted_optimizations();
  optimization2->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_hint1 =
      optimization2->add_resource_loading_hints();
  resource_hint1->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_hint1->set_resource_pattern("resource.js");

  // Add a second, non-experimental hint.
  optimization_guide::proto::Hint* hint2 = config.add_hints();
  hint2->set_key("twitter.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint3 = hint2->add_page_hints();
  page_hint3->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization3 =
      page_hint3->add_whitelisted_optimizations();
  optimization3->set_optimization_type(optimization_guide::proto::NOSCRIPT);
  ProcessHints(config, "2.0.0");

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  // Check to ensure the optimization under test (facebook noscript) is either
  // enabled or disabled, depending on what the caller told us to expect.
  EXPECT_EQ(expect_enabled, MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
                                &user_data, GURL("https://m.facebook.com"),
                                PreviewsType::NOSCRIPT, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);

  // RESOURCE_LOADING_HINTS for facebook should always be enabled.
  ect_threshold = net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  EXPECT_EQ(!expect_enabled,
            MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
                &user_data, GURL("https://m.facebook.com"),
                PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);
  // Twitter's NOSCRIPT should always be enabled; RESOURCE_LOADING_HINTS is not
  // configured and should be disabled.
  ect_threshold = net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.twitter.com/example"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);
  // Google (which is not configured at all) should always have both NOSCRIPT
  // and RESOURCE_LOADING_HINTS disabled.
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://google.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest,
       HandlesExperimentalFlagWithNoExperimentFlaggedOrEnabled) {
  // With the optimization NOT flagged as experimental and no experiment
  // enabled, the optimization should be enabled.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(features::kOptimizationHintsExperiments);
  DoExperimentFlagTest(base::nullopt, true);
}

TEST_F(PreviewsOptimizationGuideTest,
       HandlesExperimentalFlagWithEmptyExperimentName) {
  // Empty experiment names should be equivalent to no experiment flag set.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(features::kOptimizationHintsExperiments);
  DoExperimentFlagTest("", true);
}

TEST_F(PreviewsOptimizationGuideTest,
       HandlesExperimentalFlagWithExperimentConfiguredAndNotRunning) {
  // With the optimization flagged as experimental and no experiment
  // enabled, the optimization should be disabled.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(features::kOptimizationHintsExperiments);
  DoExperimentFlagTest("foo_experiment", false);
}

TEST_F(PreviewsOptimizationGuideTest,
       HandlesExperimentalFlagWithExperimentConfiguredAndSameOneRunning) {
  // With the optimization flagged as experimental and an experiment with that
  // name running, the optimization should be enabled.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationHintsExperiments,
      {{"experiment_name", "foo_experiment"}});
  DoExperimentFlagTest("foo_experiment", true);
}

TEST_F(PreviewsOptimizationGuideTest,
       HandlesExperimentalFlagWithExperimentConfiguredAndDifferentOneRunning) {
  // With the optimization flagged as experimental and a *different* experiment
  // enabled, the optimization should be disabled.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationHintsExperiments,
      {{"experiment_name", "bar_experiment"}});
  DoExperimentFlagTest("foo_experiment", false);
}

TEST_F(PreviewsOptimizationGuideTest, EnsureExperimentsDisabledByDefault) {
  // Mark an optimization as experiment, and ensure it's disabled even though we
  // don't explicitly enable or disable the feature as part of the test. This
  // ensures the experiments feature is disabled by default.
  DoExperimentFlagTest("foo_experiment", false);
}

TEST_F(PreviewsOptimizationGuideTest, ProcessHintsUnsupportedKeyRepIsIgnored) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("facebook.com");
  hint->set_key_representation(
      optimization_guide::proto::REPRESENTATION_UNSPECIFIED);
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization =
      page_hint->add_whitelisted_optimizations();
  optimization->set_optimization_type(optimization_guide::proto::NOSCRIPT);
  ProcessHints(config, "2.0.0");

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsUnsupportedOptimizationIsIgnored) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("facebook.com");
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::TYPE_UNSPECIFIED);

  ProcessHints(config, "2.0.0");

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest, ProcessHintsWithExistingSentinel) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});
  base::HistogramTester histogram_tester;

  // Create valid config.
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  // Create sentinel file for version 2.0.0.
  const base::FilePath sentinel_path =
      temp_dir().Append(FILE_PATH_LITERAL("previews_config_sentinel.txt"));
  base::WriteFile(sentinel_path, "2.0.0", 5);

  // Verify config not processed for version 2.0.0 (same as sentinel).
  ProcessHints(config, "2.0.0");

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_TRUE(base::PathExists(sentinel_path));
  histogram_tester.ExpectUniqueSample("Previews.ProcessHintsResult",
                                      2 /* kFailedFinishProcessing */, 1);

  // Now verify config is processed for different version and sentinel cleared.
  ProcessHints(config, "3.0.0");

  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, ect_threshold);
  EXPECT_FALSE(base::PathExists(sentinel_path));
  histogram_tester.ExpectBucketCount("Previews.ProcessHintsResult",
                                     1 /* kProcessedPreviewsHints */, 1);
}

TEST_F(PreviewsOptimizationGuideTest, ProcessHintsWithInvalidSentinelFile) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});
  base::HistogramTester histogram_tester;

  // Create valid config.
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  // Create sentinel file with invalid contents.
  const base::FilePath sentinel_path =
      temp_dir().Append(FILE_PATH_LITERAL("previews_config_sentinel.txt"));
  base::WriteFile(sentinel_path, "bad-2.0.0", 5);

  // Verify config not processed for existing sentinel with bad value but
  // that the existinel sentinel file is deleted.
  ProcessHints(config, "2.0.0");

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_FALSE(base::PathExists(sentinel_path));
  histogram_tester.ExpectUniqueSample("Previews.ProcessHintsResult",
                                      2 /* kFailedFinishProcessing */, 1);

  // Now verify config is processed with sentinel cleared.
  ProcessHints(config, "2.0.0");

  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_FALSE(base::PathExists(sentinel_path));
  histogram_tester.ExpectBucketCount("Previews.ProcessHintsResult",
                                     1 /* kProcessedPreviewsHints */, 1);
}

TEST_F(PreviewsOptimizationGuideTest, SkipHintProcessingForSameConfigVersion) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});
  base::HistogramTester histogram_tester;

  optimization_guide::proto::Configuration config1;
  optimization_guide::proto::Hint* hint1 = config1.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  optimization_guide::proto::Configuration config2;
  optimization_guide::proto::Hint* hint2 = config2.add_hints();
  hint2->set_key("google.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint2 = hint2->add_page_hints();
  page_hint2->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization2 =
      page_hint2->add_whitelisted_optimizations();
  optimization2->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;

  // Process the new hints config and verify that they are available.
  ProcessHints(config1, "2.0.0");

  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.google.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  histogram_tester.ExpectUniqueSample("Previews.ProcessHintsResult",
                                      1 /* kProcessedPreviewsHints */, 1);

  // Verify hint config with the same version as the previously processed config
  // is skipped.
  ProcessHints(config2, "2.0.0");

  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.google.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  histogram_tester.ExpectBucketCount("Previews.ProcessHintsResult",
                                     3 /* kSkippedProcessingPreviewsHints */,
                                     1);
}

TEST_F(PreviewsOptimizationGuideTest,
       SkipHintProcessingForEarlierConfigVersion) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});
  base::HistogramTester histogram_tester;

  optimization_guide::proto::Configuration config1;
  optimization_guide::proto::Hint* hint1 = config1.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  optimization_guide::proto::Configuration config2;
  optimization_guide::proto::Hint* hint2 = config2.add_hints();
  hint2->set_key("google.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint2 = hint2->add_page_hints();
  page_hint2->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization2 =
      page_hint2->add_whitelisted_optimizations();
  optimization2->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;

  // Process the new hints config and verify that they are available.
  ProcessHints(config1, "2.0.0");

  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.google.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  histogram_tester.ExpectUniqueSample("Previews.ProcessHintsResult",
                                      1 /* kProcessedPreviewsHints */, 1);

  // Verify hint config with an earlier version than the previously processed
  // one is skipped.
  ProcessHints(config2, "1.0.0");

  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.google.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  histogram_tester.ExpectBucketCount("Previews.ProcessHintsResult",
                                     3 /* kSkippedProcessingPreviewsHints */,
                                     1);
}

TEST_F(PreviewsOptimizationGuideTest, ProcessMultipleNewConfigs) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures(
      {features::kNoScriptPreviews, features::kResourceLoadingHints}, {});

  optimization_guide::proto::Configuration config1;
  optimization_guide::proto::Hint* hint1 = config1.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  optimization_guide::proto::Configuration config2;
  optimization_guide::proto::Hint* hint2 = config2.add_hints();
  hint2->set_key("google.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint2 = hint2->add_page_hints();
  page_hint2->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization2 =
      page_hint2->add_whitelisted_optimizations();
  optimization2->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;

  // Process the new hints config and verify that they are available.
  ProcessHints(config1, "2.0.0");

  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.google.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  histogram_tester.ExpectUniqueSample("Previews.ProcessHintsResult",
                                      1 /* kProcessedPreviewsHints */, 1);

  // Verify hint config with a newer version then the previously processed one
  // is processed.
  ProcessHints(config2, "3.0.0");

  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://m.google.com"), PreviewsType::NOSCRIPT,
      &ect_threshold));
  histogram_tester.ExpectBucketCount("Previews.ProcessHintsResult",
                                     1 /* kProcessedPreviewsHints */, 2);
}

TEST_F(PreviewsOptimizationGuideTest, ProcessHintConfigWithNoKeyFailsDcheck) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  EXPECT_DCHECK_DEATH({
    ProcessHints(config, "2.0.0");
  });
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsConfigWithDuplicateKeysFailsDcheck) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);
  optimization_guide::proto::Hint* hint2 = config.add_hints();
  hint2->set_key("facebook.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint2 = hint1->add_page_hints();
  page_hint2->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization2 =
      page_hint2->add_whitelisted_optimizations();
  optimization2->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  EXPECT_DCHECK_DEATH({
    ProcessHints(config, "2.0.0");
  });
}

TEST_F(PreviewsOptimizationGuideTest, MaybeLoadOptimizationHints) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  InitializeFixedCountResourceLoadingHints();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.unknown.com"), base::DoNothing()));

  RunUntilIdle();

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  // Verify whitelisting from loaded page hints.
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/news/weather/raininginseattle"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/football/seahawksrebuildingyear"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain.org/unhinted"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest,
       MaybeLoadPageHintsWithTwoExperimentsDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  InitializeFixedCountResourceLoadingHintsWithTwoExperiments();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.unknown.com"), base::DoNothing()));

  RunUntilIdle();

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  // Verify whitelisting from loaded page hints.
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/news/weather/raininginseattle"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/football/seahawksrebuildingyear"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain.org/unhinted"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest,
       MaybeLoadPageHintsWithFirstExperimentEnabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  base::test::ScopedFeatureList scoped_list2;
  scoped_list2.InitAndEnableFeatureWithParameters(
      features::kOptimizationHintsExperiments,
      {{"experiment_name", "experiment_1"}});

  InitializeFixedCountResourceLoadingHintsWithTwoExperiments();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.unknown.com"), base::DoNothing()));

  RunUntilIdle();

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  // Verify whitelisting from loaded page hints.
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/news/weather/raininginseattle"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/football/seahawksrebuildingyear"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain.org/unhinted"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest,
       MaybeLoadPageHintsWithSecondExperimentEnabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  base::test::ScopedFeatureList scoped_list2;
  scoped_list2.InitAndEnableFeatureWithParameters(
      features::kOptimizationHintsExperiments,
      {{"experiment_name", "experiment_2"}});

  InitializeFixedCountResourceLoadingHintsWithTwoExperiments();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.unknown.com"), base::DoNothing()));

  RunUntilIdle();

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  // Verify whitelisting from loaded page hints.
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/news/weather/raininginseattle"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/football/seahawksrebuildingyear"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain.org/unhinted"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest,
       MaybeLoadPageHintsWithBothExperimentEnabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  base::test::ScopedFeatureList scoped_list2;
  scoped_list2.InitAndEnableFeatureWithParameters(
      features::kOptimizationHintsExperiments,
      {{"experiment_name", "experiment_1"},
       {"experiment_name", "experiment_2"}});

  InitializeFixedCountResourceLoadingHintsWithTwoExperiments();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.unknown.com"), base::DoNothing()));

  RunUntilIdle();

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  // Verify whitelisting from loaded page hints.
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/news/weather/raininginseattle"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/football/seahawksrebuildingyear"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain.org/unhinted"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
}

// Test that optimization hints with multiple page patterns is processed
// correctly.
TEST_F(PreviewsOptimizationGuideTest,
       LoadManyResourceLoadingOptimizationHints) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  const size_t key_count = 50;
  const size_t page_patterns_per_key = 25;

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;

  InitializeMultipleResourceLoadingHints(key_count, page_patterns_per_key);

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain0.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain0.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain0.org/news0/football"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain49.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain49.org/news0/football"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain50.org/"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain50.org/news0/football"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.unknown.com"), base::DoNothing()));

  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain0.org/news0/football"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain0.org/news24/football"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain0.org/news25/football"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));

  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain49.org/news0/football"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_TRUE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain49.org/news24/football"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain49.org/news25/football"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));

  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain50.org/news0/football"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain50.org/news24/football"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data, GURL("https://www.somedomain50.org/news25/football"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));

  RunUntilIdle();
}

TEST_F(PreviewsOptimizationGuideTest,
       MaybeLoadOptimizationHintsWithoutEnabledPageHintsFeature) {
  // Without PageHints-oriented feature enabled, never see
  // enabled, the optimization should be disabled.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(features::kResourceLoadingHints);

  InitializeFixedCountResourceLoadingHints();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org"), base::DoNothing()));

  RunUntilIdle();

  PreviewsUserData user_data(kDefaultPageId);
  net::EffectiveConnectionType ect_threshold;
  EXPECT_FALSE(MaybeLoadOptimizationHintsAndCheckIsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/news/weather/raininginseattle"),
      PreviewsType::RESOURCE_LOADING_HINTS, &ect_threshold));
}

TEST_F(PreviewsOptimizationGuideTest, IsBlacklisted) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kLitePageServerPreviews);

  EXPECT_TRUE(
      guide()->IsBlacklisted(GURL("https://m.blacklisteddomain.com/path"),
                             PreviewsType::LITE_PAGE_REDIRECT));

  InitializeWithLitePageRedirectBlacklist();

  EXPECT_TRUE(
      guide()->IsBlacklisted(GURL("https://m.blacklisteddomain.com/path"),
                             PreviewsType::LITE_PAGE_REDIRECT));
  EXPECT_DCHECK_DEATH(guide()->IsBlacklisted(
      GURL("https://m.blacklisteddomain.com/path"), PreviewsType::NOSCRIPT));

  EXPECT_TRUE(guide()->IsBlacklisted(
      GURL("https://blacklistedsubdomain.maindomain.co.in"),
      PreviewsType::LITE_PAGE_REDIRECT));

  EXPECT_FALSE(guide()->IsBlacklisted(GURL("https://maindomain.co.in"),
                                      PreviewsType::LITE_PAGE_REDIRECT));
}

TEST_F(PreviewsOptimizationGuideTest, LitePageRedirectSkipIsBlacklistedCheck) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kLitePageServerPreviews);
  InitializeWithLitePageRedirectBlacklist();

  EXPECT_TRUE(
      guide()->IsBlacklisted(GURL("https://m.blacklisteddomain.com/path"),
                             PreviewsType::LITE_PAGE_REDIRECT));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kIgnoreLitePageRedirectOptimizationBlacklist);

  EXPECT_FALSE(
      guide()->IsBlacklisted(GURL("https://m.blacklisteddomain.com/path"),
                             PreviewsType::LITE_PAGE_REDIRECT));
}

TEST_F(PreviewsOptimizationGuideTest,
       IsBlacklistedWithLitePageServerPreviewsDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(features::kLitePageServerPreviews);

  InitializeWithLitePageRedirectBlacklist();

  EXPECT_TRUE(
      guide()->IsBlacklisted(GURL("https://m.blacklisteddomain.com/path"),
                             PreviewsType::LITE_PAGE_REDIRECT));
}

TEST_F(PreviewsOptimizationGuideTest, RemoveObserverCalledAtDestruction) {
  EXPECT_FALSE(optimization_guide_service()->RemoveObserverCalled());

  ResetGuide();

  EXPECT_TRUE(optimization_guide_service()->RemoveObserverCalled());
}

}  // namespace previews
