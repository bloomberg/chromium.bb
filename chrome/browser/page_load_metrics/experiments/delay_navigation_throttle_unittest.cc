// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/experiments/delay_navigation_throttle.h"

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class DelayNavigationThrottleTest : public ChromeRenderViewHostTestHarness {};

TEST_F(DelayNavigationThrottleTest, BasicDelay) {
  const char kBasicResponseHeaders[] = "HTTP/1.1 200 OK";
  base::TimeDelta navigation_delay = base::TimeDelta::FromSeconds(1);
  GURL url("http://www.example.com/");

  scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner(
      new base::TestMockTimeTaskRunner());
  std::unique_ptr<content::NavigationHandle> test_handle =
      content::NavigationHandle::CreateNavigationHandleForTesting(url,
                                                                  main_rfh());
  test_handle->RegisterThrottleForTesting(
      base::MakeUnique<DelayNavigationThrottle>(
          test_handle.get(), mock_time_task_runner, navigation_delay));

  EXPECT_FALSE(mock_time_task_runner->HasPendingTask());
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            test_handle->CallWillStartRequestForTesting(
                false /* is_post */, content::Referrer(),
                false /* has_user_gesture */, ui::PAGE_TRANSITION_LINK,
                false /* is_external_protocol */));

  // There may be other throttles that DEFER and post async tasks to the UI
  // thread. Allow them to run to completion, so our throttle is guaranteed to
  // have a chance to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(mock_time_task_runner->HasPendingTask());
  EXPECT_FALSE(test_handle->HasCommitted());

  mock_time_task_runner->FastForwardBy(navigation_delay);
  EXPECT_FALSE(mock_time_task_runner->HasPendingTask());

  // Run any remaining async tasks, to make sure all other deferred throttles
  // can complete.
  base::RunLoop().RunUntilIdle();

  // Verify that the WillSendRequest portion of the navigation has completed,
  // and NavigationHandle::WillProcessResponse and the commit portion of the
  // navigation lifetime can now be invoked.
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            test_handle->CallWillProcessResponseForTesting(
                main_rfh(),
                net::HttpUtil::AssembleRawHeaders(
                    kBasicResponseHeaders, strlen(kBasicResponseHeaders))));
  test_handle->CallDidCommitNavigationForTesting(url);
  EXPECT_TRUE(test_handle->HasCommitted());
}

enum ExpectInstantiationResult {
  EXPECT_INSTANTIATION,
  EXPECT_NO_INSTANTIATION
};

class DelayNavigationThrottleInstantiationTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<
          std::tuple<ExpectInstantiationResult,
                     base::FeatureList::OverrideState,
                     base::TimeDelta,
                     bool,
                     double,
                     GURL>> {
 public:
  DelayNavigationThrottleInstantiationTest()
      : field_trial_list_(nullptr /* entropy_provider */) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    std::tie(expected_instantiation_result_, feature_state_, param_delay_,
             param_randomize_delay_, param_probability_, url_) = GetParam();

    std::map<std::string, std::string> variation_params(
        {{DelayNavigationThrottle::kParamDelayNavigationDurationMillis,
          base::IntToString(param_delay_.InMilliseconds())},
         {DelayNavigationThrottle::kParamDelayNavigationRandomize,
          param_randomize_delay_ ? "true" : "false"},
         {DelayNavigationThrottle::kParamDelayNavigationProbability,
          base::DoubleToString(param_probability_)}});
    InitializeScopedFeatureList(feature_state_, variation_params);
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
    variations::testing::ClearAllVariationParams();
  }

  void InitializeScopedFeatureList(
      base::FeatureList::OverrideState feature_state,
      const std::map<std::string, std::string>& variation_params) {
    static const char kTestFieldTrialName[] = "TestTrial";
    static const char kTestExperimentGroupName[] = "TestGroup";

    EXPECT_TRUE(variations::AssociateVariationParams(
        kTestFieldTrialName, kTestExperimentGroupName, variation_params));

    base::FieldTrial* field_trial = base::FieldTrialList::CreateFieldTrial(
        kTestFieldTrialName, kTestExperimentGroupName);

    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->RegisterFieldTrialOverride(kDelayNavigationFeature.name,
                                             feature_state, field_trial);

    // Since we are adding a scoped feature list after browser start, copy over
    // the existing feature list to prevent inconsistency.
    base::FeatureList* existing_feature_list = base::FeatureList::GetInstance();
    if (existing_feature_list) {
      std::string enabled_features;
      std::string disabled_features;
      base::FeatureList::GetInstance()->GetFeatureOverrides(&enabled_features,
                                                            &disabled_features);
      feature_list->InitializeFromCommandLine(enabled_features,
                                              disabled_features);
    }

    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }

  base::FieldTrialList field_trial_list_;
  base::test::ScopedFeatureList scoped_feature_list_;

  // Fields filled in via GetParam():
  base::FeatureList::OverrideState feature_state_;
  base::TimeDelta param_delay_;
  bool param_randomize_delay_ = false;
  double param_probability_ = 0;
  GURL url_;
  ExpectInstantiationResult expected_instantiation_result_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DelayNavigationThrottleInstantiationTest);
};

TEST_P(DelayNavigationThrottleInstantiationTest, Instantiate) {
  std::unique_ptr<content::NavigationHandle> test_handle =
      content::NavigationHandle::CreateNavigationHandleForTesting(url_,
                                                                  main_rfh());
  std::unique_ptr<DelayNavigationThrottle> throttle =
      DelayNavigationThrottle::MaybeCreateThrottleFor(test_handle.get());
  const bool expect_instantiation =
      expected_instantiation_result_ == EXPECT_INSTANTIATION;
  EXPECT_EQ(expect_instantiation, throttle != nullptr);
  if (throttle) {
    base::TimeDelta delay = throttle->navigation_delay();
    EXPECT_FALSE(delay.is_zero());
    EXPECT_GE(param_delay_, delay);
  }
}

INSTANTIATE_TEST_CASE_P(
    InstantiateThrottle,
    DelayNavigationThrottleInstantiationTest,
    ::testing::Values(
        std::make_tuple(EXPECT_NO_INSTANTIATION,
                        base::FeatureList::OVERRIDE_DISABLE_FEATURE,
                        base::TimeDelta::FromMilliseconds(10),
                        false /* randomize delay */,
                        1.0 /* delay probability */,
                        GURL("http://www.example.com/")),
        std::make_tuple(EXPECT_NO_INSTANTIATION,
                        base::FeatureList::OVERRIDE_ENABLE_FEATURE,
                        base::TimeDelta::FromMilliseconds(10),
                        false /* randomize delay */,
                        1.0 /* delay probability */,
                        GURL("chrome://version")),
        std::make_tuple(EXPECT_NO_INSTANTIATION,
                        base::FeatureList::OVERRIDE_ENABLE_FEATURE,
                        base::TimeDelta::FromMilliseconds(10),
                        false /* randomize delay */,
                        0.0 /* delay probability */,
                        GURL("http://www.example.com/")),
        std::make_tuple(EXPECT_INSTANTIATION,
                        base::FeatureList::OVERRIDE_ENABLE_FEATURE,
                        base::TimeDelta::FromMilliseconds(10),
                        false /* randomize delay */,
                        1.0 /* delay probability */,
                        GURL("http://www.example.com/")),
        std::make_tuple(EXPECT_INSTANTIATION,
                        base::FeatureList::OVERRIDE_ENABLE_FEATURE,
                        base::TimeDelta::FromMilliseconds(10),
                        true /* randomize delay */,
                        1.0 /* delay probability */,
                        GURL("http://www.example.com/"))));
