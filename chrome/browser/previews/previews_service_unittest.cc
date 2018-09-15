// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_service.h"

#include <initializer_list>
#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/previews/content/previews_decider_impl.h"
#include "components/previews/content/previews_optimization_guide.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_features.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This test class intercepts the |is_enabled_callback| and is used to test its
// validity.
class TestPreviewsDeciderImpl : public previews::PreviewsDeciderImpl {
 public:
  TestPreviewsDeciderImpl()
      : previews::PreviewsDeciderImpl(
            content::BrowserThread::GetTaskRunnerForThread(
                content::BrowserThread::UI),
            content::BrowserThread::GetTaskRunnerForThread(
                content::BrowserThread::UI),
            base::DefaultClock::GetInstance()) {}
  ~TestPreviewsDeciderImpl() override {}

  void Initialize(
      base::WeakPtr<previews::PreviewsUIService> previews_ui_service,
      std::unique_ptr<blacklist::OptOutStore> previews_opt_out_store,
      std::unique_ptr<previews::PreviewsOptimizationGuide> previews_opt_guide,
      const previews::PreviewsIsEnabledCallback& is_enabled_callback,
      blacklist::BlacklistData::AllowedTypesAndVersions allowed_previews)
      override {
    enabled_callback_ = is_enabled_callback;
  }

  bool IsPreviewEnabled(previews::PreviewsType type) {
    return enabled_callback_.Run(type);
  }

 private:
  previews::PreviewsIsEnabledCallback enabled_callback_;
};

// Class to test the validity of the callback passed to PreviewsDeciderImpl from
// PreviewsService.
class PreviewsServiceTest : public testing::Test {
 public:
  PreviewsServiceTest()

      : field_trial_list_(nullptr), scoped_feature_list_() {}

  void SetUp() override {
    previews_decider_impl_ = std::make_unique<TestPreviewsDeciderImpl>();

    service_ = std::make_unique<PreviewsService>(nullptr);
    base::FilePath file_path;
    service_->Initialize(previews_decider_impl_.get(),
                         nullptr /* optimization_guide_service */,
                         content::BrowserThread::GetTaskRunnerForThread(
                             content::BrowserThread::UI),
                         file_path);
    scoped_feature_list_.InitWithFeatures(
        {previews::features::kPreviews},
        {data_reduction_proxy::features::kDataReductionProxyDecidesTransform});
  }

  void TearDown() override { variations::testing::ClearAllVariationParams(); }

  ~PreviewsServiceTest() override {}

  TestPreviewsDeciderImpl* previews_decider_impl() const {
    return previews_decider_impl_.get();
  }

 private:
  content::TestBrowserThreadBundle threads_;
  base::FieldTrialList field_trial_list_;
  std::unique_ptr<TestPreviewsDeciderImpl> previews_decider_impl_;
  std::unique_ptr<PreviewsService> service_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

TEST_F(PreviewsServiceTest, TestOfflineFieldTrialNotSet) {
  EXPECT_TRUE(previews_decider_impl()->IsPreviewEnabled(
      previews::PreviewsType::OFFLINE));
}

TEST_F(PreviewsServiceTest, TestOfflineFeatureDisabled) {
  std::unique_ptr<base::FeatureList> feature_list =
      std::make_unique<base::FeatureList>();

  // The feature is explicitly enabled on the command-line.
  feature_list->InitializeFromCommandLine("", "OfflinePreviews");
  base::FeatureList::ClearInstanceForTesting();
  base::FeatureList::SetInstance(std::move(feature_list));

  EXPECT_FALSE(previews_decider_impl()->IsPreviewEnabled(
      previews::PreviewsType::OFFLINE));
  base::FeatureList::ClearInstanceForTesting();
}

TEST_F(PreviewsServiceTest, TestClientLoFiFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kPreviews,
       previews::features::kClientLoFi} /* enabled features */,
      {data_reduction_proxy::features::
           kDataReductionProxyDecidesTransform} /* disabled features */);

  base::FieldTrialList::CreateFieldTrial("PreviewsClientLoFi", "Enabled");
  EXPECT_TRUE(
      previews_decider_impl()->IsPreviewEnabled(previews::PreviewsType::LOFI));
}

TEST_F(PreviewsServiceTest, TestClientLoFiAndServerLoFiEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kPreviews, previews::features::kClientLoFi,
       data_reduction_proxy::features::
           kDataReductionProxyDecidesTransform} /* enabled features */,
      {} /* disabled features */);

  EXPECT_TRUE(
      previews_decider_impl()->IsPreviewEnabled(previews::PreviewsType::LOFI));
}

TEST_F(PreviewsServiceTest, TestClientLoFiAndServerLoFiNotEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kPreviews} /* enabled features */,
      {previews::features::kClientLoFi,
       data_reduction_proxy::features::
           kDataReductionProxyDecidesTransform} /* disabled features */);
  EXPECT_FALSE(
      previews_decider_impl()->IsPreviewEnabled(previews::PreviewsType::LOFI));
}

TEST_F(PreviewsServiceTest, TestLitePageNotEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kPreviews} /* enabled features */,
      {data_reduction_proxy::features::
           kDataReductionProxyDecidesTransform} /* disabled features */);
  EXPECT_FALSE(previews_decider_impl()->IsPreviewEnabled(
      previews::PreviewsType::LITE_PAGE));
}

TEST_F(PreviewsServiceTest, TestServerLoFiProxyDecidesTransform) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kPreviews,
       data_reduction_proxy::features::kDataReductionProxyDecidesTransform},
      {});
  EXPECT_TRUE(
      previews_decider_impl()->IsPreviewEnabled(previews::PreviewsType::LOFI));
}

TEST_F(PreviewsServiceTest, TestLitePageProxyDecidesTransform) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kPreviews,
       data_reduction_proxy::features::kDataReductionProxyDecidesTransform},
      {});
  EXPECT_TRUE(previews_decider_impl()->IsPreviewEnabled(
      previews::PreviewsType::LITE_PAGE));
}

TEST_F(PreviewsServiceTest, TestNoScriptPreviewsEnabledByFeature) {
#if !defined(OS_ANDROID)
  // For non-android, default is disabled.
  EXPECT_FALSE(previews_decider_impl()->IsPreviewEnabled(
      previews::PreviewsType::NOSCRIPT));
#endif  // defined(OS_ANDROID)

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      previews::features::kNoScriptPreviews);
  EXPECT_TRUE(previews_decider_impl()->IsPreviewEnabled(
      previews::PreviewsType::NOSCRIPT));
}
