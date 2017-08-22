// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/ad_sampler_trigger.h"

#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/triggers/trigger_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock-generated-function-mockers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::NavigationSimulator;
using content::RenderFrameHost;
using content::RenderFrameHostTester;

using testing::_;
using testing::Return;

namespace safe_browsing {

namespace {
const char kAdUrl[] = "https://tpc.googlesyndication.com/safeframe/1";
const char kNonAdUrl[] = "https://foo.com/";
const char kAdName[] = "google_ads_iframe_1";
const char kNonAdName[] = "foo";
}  // namespace

class MockTriggerManager : public TriggerManager {
 public:
  MockTriggerManager() : TriggerManager(nullptr) {}

  MOCK_METHOD6(StartCollectingThreatDetails,
               bool(TriggerType trigger_type,
                    content::WebContents* web_contents,
                    const security_interstitials::UnsafeResource& resource,
                    net::URLRequestContextGetter* request_context_getter,
                    history::HistoryService* history_service,
                    const SBErrorOptions& error_display_options));

  MOCK_METHOD6(FinishCollectingThreatDetails,
               bool(TriggerType trigger_type,
                    content::WebContents* web_contents,
                    const base::TimeDelta& delay,
                    bool did_proceed,
                    int num_visits,
                    const SBErrorOptions& error_display_options));
};

class AdSamplerTriggerTest : public content::RenderViewHostTestHarness {
 public:
  AdSamplerTriggerTest() {}
  ~AdSamplerTriggerTest() override {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    // Enable any prefs required for the trigger to run.
    safe_browsing::RegisterProfilePrefs(prefs_.registry());
    prefs_.SetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed, true);
    prefs_.SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled, true);
    prefs_.SetBoolean(prefs::kSafeBrowsingScoutGroupSelected, true);
  }

  void CreateTriggerWithFrequency(const size_t denominator) {
    safe_browsing::AdSamplerTrigger::CreateForWebContents(
        web_contents(), &trigger_manager_, &prefs_, nullptr, nullptr);
    safe_browsing::AdSamplerTrigger::FromWebContents(web_contents())
        ->sampler_frequency_denominator_ = denominator;
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* NavigateFrame(const std::string& url,
                                 RenderFrameHost* frame) {
    auto navigation_simulator =
        NavigationSimulator::CreateRendererInitiated(GURL(url), frame);
    navigation_simulator->Commit();
    return navigation_simulator->GetFinalRenderFrameHost();
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* NavigateMainFrame(const std::string& url) {
    return NavigateFrame(url, web_contents()->GetMainFrame());
  }
  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* CreateAndNavigateSubFrame(const std::string& url,
                                             const std::string& frame_name,
                                             RenderFrameHost* parent) {
    RenderFrameHost* subframe =
        RenderFrameHostTester::For(parent)->AppendChild(frame_name);
    auto navigation_simulator =
        NavigationSimulator::CreateRendererInitiated(GURL(url), subframe);
    navigation_simulator->Commit();
    return navigation_simulator->GetFinalRenderFrameHost();
  }

  MockTriggerManager* get_trigger_manager() { return &trigger_manager_; }

 private:
  TestingPrefServiceSimple prefs_;
  MockTriggerManager trigger_manager_;
};

TEST_F(AdSamplerTriggerTest, TriggerDisabledBySamplingFrequency) {
  // Make sure the trigger doesn't fire when the samlping frequency is set to
  // zero, which disables the trigger.
  CreateTriggerWithFrequency(kSamplerFrequencyDisabled);
  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetails(_, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(_, _, _, _, _, _))
      .Times(0);

  // This page contains two ads - one identifiable by its URL, the other by the
  // name of the frame.
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  CreateAndNavigateSubFrame(kAdUrl, kNonAdName, main_frame);
  CreateAndNavigateSubFrame(kNonAdUrl, kAdName, main_frame);
}

TEST_F(AdSamplerTriggerTest, PageWithNoAds) {
  // Make sure the trigger doesn't fire when there are no ads on the page.
  CreateTriggerWithFrequency(/*denominator=*/1);

  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetails(_, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(_, _, _, _, _, _))
      .Times(0);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame);
  CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame);
}

TEST_F(AdSamplerTriggerTest, PageWithMultipleAds) {
  // Make sure the trigger fires when there are ads on the page. We expect
  // one call for each ad detected.
  CreateTriggerWithFrequency(/*denominator=*/1);
  EXPECT_CALL(*get_trigger_manager(),
              StartCollectingThreatDetails(TriggerType::AD_SAMPLE,
                                           web_contents(), _, _, _, _))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*get_trigger_manager(),
              FinishCollectingThreatDetails(TriggerType::AD_SAMPLE,
                                            web_contents(), _, _, _, _))
      .Times(2);

  // This page contains two ads - one identifiable by its URL, the other by the
  // name of the frame.
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  CreateAndNavigateSubFrame(kAdUrl, kNonAdName, main_frame);
  CreateAndNavigateSubFrame(kNonAdUrl, kAdName, main_frame);
}

TEST(AdSamplerTriggerTestFinch, FrequencyDenominatorFeature) {
  // Make sure that setting the frequency denominator via Finch params works as
  // expected.
  const size_t kDenominatorInt = 12345;
  base::FieldTrialList field_trial_list(nullptr);

  base::FieldTrial* trial = base::FieldTrialList::CreateFieldTrial(
      safe_browsing::kAdSamplerTriggerFeature.name, "Group");
  std::map<std::string, std::string> feature_params;
  feature_params[std::string(
      safe_browsing::kAdSamplerFrequencyDenominatorParam)] =
      base::IntToString(kDenominatorInt);
  base::AssociateFieldTrialParams(safe_browsing::kAdSamplerTriggerFeature.name,
                                  "Group", feature_params);
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->InitializeFromCommandLine(
      safe_browsing::kAdSamplerTriggerFeature.name, std::string());
  feature_list->AssociateReportingFieldTrial(
      safe_browsing::kAdSamplerTriggerFeature.name,
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  AdSamplerTrigger trigger(nullptr, nullptr, nullptr, nullptr, nullptr);
  EXPECT_EQ(kDenominatorInt, trigger.sampler_frequency_denominator_);
}
}  // namespace safe_browsing
