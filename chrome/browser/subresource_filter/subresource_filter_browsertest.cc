// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/subresource_filter/test_ruleset_publisher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "components/subresource_filter/core/common/scoped_timers.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// The path to a multi-frame document used for tests.
static constexpr const char kTestFrameSetPath[] =
    "subresource_filter/frame_set.html";

// Names of DocumentLoad histograms.
constexpr const char kDocumentLoadActivationState[] =
    "SubresourceFilter.DocumentLoad.ActivationState";
constexpr const char kSubresourceLoadsTotal[] =
    "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Total";
constexpr const char kSubresourceLoadsEvaluated[] =
    "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Evaluated";
constexpr const char kSubresourceLoadsMatchedRules[] =
    "SubresourceFilter.DocumentLoad.NumSubresourceLoads.MatchedRules";
constexpr const char kSubresourceLoadsDisallowed[] =
    "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Disallowed";

// Names of the performance measurement histograms.
constexpr const char kActivationWallDuration[] =
    "SubresourceFilter.DocumentLoad.Activation.WallDuration";
constexpr const char kActivationCPUDuration[] =
    "SubresourceFilter.DocumentLoad.Activation.CPUDuration";
constexpr const char kEvaluationTotalWallDurationForPage[] =
    "SubresourceFilter.PageLoad.SubresourceEvaluation.TotalWallDuration";
constexpr const char kEvaluationTotalCPUDurationForPage[] =
    "SubresourceFilter.PageLoad.SubresourceEvaluation.TotalCPUDuration";
constexpr const char kEvaluationTotalWallDurationForDocument[] =
    "SubresourceFilter.DocumentLoad.SubresourceEvaluation.TotalWallDuration";
constexpr const char kEvaluationTotalCPUDurationForDocument[] =
    "SubresourceFilter.DocumentLoad.SubresourceEvaluation.TotalCPUDuration";
constexpr const char kEvaluationWallDuration[] =
    "SubresourceFilter.SubresourceLoad.Evaluation.WallDuration";
constexpr const char kEvaluationCPUDuration[] =
    "SubresourceFilter.SubresourceLoad.Evaluation.CPUDuration";

// Other histograms.
const char kSubresourceFilterPromptHistogram[] =
    "SubresourceFilter.Prompt.NumVisibility";

GURL GetURLWithFragment(const GURL& url, base::StringPiece fragment) {
  GURL::Replacements replacements;
  replacements.SetRefStr(fragment);
  return url.ReplaceComponents(replacements);
}

}  // namespace

namespace subresource_filter {

using subresource_filter::testing::ScopedSubresourceFilterFeatureToggle;
using subresource_filter::testing::TestRulesetPublisher;

// SubresourceFilterDisabledBrowserTest ---------------------------------------

using SubresourceFilterDisabledBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SubresourceFilterDisabledBrowserTest,
                       RulesetServiceNotCreatedByDefault) {
  EXPECT_FALSE(g_browser_process->subresource_filter_ruleset_service());
}

// SubresourceFilterBrowserTest -----------------------------------------------

class SubresourceFilterBrowserTestImpl : public InProcessBrowserTest {
 public:
  explicit SubresourceFilterBrowserTestImpl(bool measure_performance)
      : measure_performance_(measure_performance) {}

  ~SubresourceFilterBrowserTestImpl() override {}

 protected:
  // It would be too late to enable the feature in SetUpOnMainThread(), as it is
  // called after ChromeBrowserMainParts::PreBrowserStart(), which instantiates
  // the RulesetService.
  //
  // On the other hand, setting up field trials in this method would be too
  // early, as it is called before BrowserMain, which expects no FieldTrialList
  // singleton to exist. There are no other hooks we could use either.
  //
  // As a workaround, enable the feature here, then enable the feature once
  // again + set up the field trials later.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                    kSafeBrowsingSubresourceFilter.name);
  }

  void SetUpOnMainThread() override {
    scoped_feature_toggle_.reset(new ScopedSubresourceFilterFeatureToggle(
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
        kActivationScopeAllSites, std::string(),
        measure_performance_ ? "1" : "0"));

    base::FilePath test_data_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetTestUrl(const std::string& path) {
    return embedded_test_server()->base_url().Resolve(path);
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* FindFrameByName(const std::string& name) {
    for (content::RenderFrameHost* frame : web_contents()->GetAllFrames()) {
      if (frame->GetFrameName() == name)
        return frame;
    }
    return nullptr;
  }

  bool WasParsedScriptElementLoaded(content::RenderFrameHost* rfh) {
    DCHECK(rfh);
    bool script_resource_was_loaded = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        rfh, "domAutomationController.send(!!document.scriptExecuted)",
        &script_resource_was_loaded));
    return script_resource_was_loaded;
  }

  bool IsDynamicScriptElementLoaded(content::RenderFrameHost* rfh) {
    DCHECK(rfh);
    bool script_resource_was_loaded = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        rfh, "insertScriptElementAndReportSuccess()",
        &script_resource_was_loaded));
    return script_resource_was_loaded;
  }

  void InsertDynamicFrameWithScript() {
    bool frame_was_loaded = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents()->GetMainFrame(), "insertFrameWithScriptAndNotify()",
        &frame_was_loaded));
    ASSERT_TRUE(frame_was_loaded);
  }

  void SetRulesetToDisallowURLsWithPathSuffix(const std::string& suffix) {
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_publisher_.SetRulesetToDisallowURLsWithPathSuffix(suffix));
  }

 private:
  std::unique_ptr<ScopedSubresourceFilterFeatureToggle> scoped_feature_toggle_;
  TestRulesetPublisher test_ruleset_publisher_;
  bool measure_performance_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterBrowserTestImpl);
};

class SubresourceFilterBrowserTest : public SubresourceFilterBrowserTestImpl {
 public:
  SubresourceFilterBrowserTest() : SubresourceFilterBrowserTestImpl(false) {}
};

// TODO(pkalinnikov): It should be possible to have only one fixture, i.e.,
// SubresourceFilterBrowserTest, unsetting |measure_performance| by default, and
// providing a method to switch it on. However, the current implementation of
// ScopedSubresourceFilterFeatureToggle in use pollutes the global
// FieldTrialList in such a way that variation parameters can not be reassigned.
class SubresourceFilterWithPerformanceMeasurementBrowserTest
    : public SubresourceFilterBrowserTestImpl {
 public:
  SubresourceFilterWithPerformanceMeasurementBrowserTest()
      : SubresourceFilterBrowserTestImpl(true) {}
};

// Tests -----------------------------------------------------------------------

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, MainFrameActivation) {
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  // The main frame document should never be filtered.
  SetRulesetToDisallowURLsWithPathSuffix("frame_with_included_script.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

// There should be no document-level de-/reactivation happening on the renderer
// side as a result of an in-page navigation.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       DocumentActivationOutlivesSamePageNavigation) {
  GURL url(GetTestUrl("subresource_filter/frame_with_delayed_script.html"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  // Deactivation would already detected by the IsDynamicScriptElementLoaded
  // line alone. To ensure no reactivation, which would muddy up recorded
  // histograms, also set a ruleset that allows everything. If there was
  // reactivation, then this new ruleset would be picked up, once again causing
  // the IsDynamicScriptElementLoaded check to fail.
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));
  ui_test_utils::NavigateToURL(browser(), GetURLWithFragment(url, "ref"));
  EXPECT_FALSE(IsDynamicScriptElementLoaded(web_contents()->GetMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, SubFrameActivation) {
  GURL url(GetTestUrl(kTestFrameSetPath));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);

  const char* kSubframeNames[] = {"one", "two", "three"};
  const bool kExpectScriptElementToLoad[arraysize(kSubframeNames)] = {
      false, true, false};
  for (size_t i = 0; i < arraysize(kSubframeNames); ++i) {
    content::RenderFrameHost* frame = FindFrameByName(kSubframeNames[i]);
    ASSERT_TRUE(frame);
    EXPECT_EQ(kExpectScriptElementToLoad[i],
              WasParsedScriptElementLoaded(frame));
  }

  tester.ExpectBucketCount(kSubresourceFilterPromptHistogram, true, 1);
}

// The page-level activation state on the browser-side should not be reset when
// a same-page navigation starts in the main frame. Verify this by dynamically
// inserting a subframe afterwards, and still expecting activation.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PageLevelActivationOutlivesSamePageNavigation) {
  GURL url(GetTestUrl(kTestFrameSetPath));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  ui_test_utils::NavigateToURL(browser(), GetURLWithFragment(url, "ref"));

  ASSERT_NO_FATAL_FAILURE(InsertDynamicFrameWithScript());
  content::RenderFrameHost* dynamic_frame = FindFrameByName("dynamic");
  ASSERT_TRUE(dynamic_frame);
  EXPECT_FALSE(WasParsedScriptElementLoaded(dynamic_frame));
}

// Schemes 'about:' and 'chrome-native:' are loaded synchronously as empty
// documents in Blink, so there is no chance (or need) to send an activation
// IPC message. Make sure that histograms are not polluted with these loads.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, NoActivationOnAboutBlank) {
  GURL url(GetTestUrl("subresource_filter/frame_set_sync_loads.html"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), url);

  content::RenderFrameHost* frame = FindFrameByName("initially_blank");
  ASSERT_TRUE(frame);
  EXPECT_FALSE(WasParsedScriptElementLoaded(frame));

  // Support both pre-/post-PersistentHistograms worlds. The latter is enabled
  // through field trials, so the former is still used on offical builders.
  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // The only frames where filtering was (even considered to be) activated
  // should be the main frame, and the child that was navigated to an HTTP URL.
  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.DocumentLoad.ActivationState",
      static_cast<base::Histogram::Sample>(ActivationState::ENABLED), 2);
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PRE_MainFrameActivationOnStartup) {
  SetRulesetToDisallowURLsWithPathSuffix("included_script.js");
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       MainFrameActivationOnStartup) {
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  // Verify that the ruleset persisted in the previous session is used for this
  // page load right after start-up.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PromptShownAgainOnNextNavigation) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL url(GetTestUrl(kTestFrameSetPath));
  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);
  tester.ExpectBucketCount(kSubresourceFilterPromptHistogram, true, 1);
  // Check that the bubble is not shown again for this navigation.
  EXPECT_FALSE(IsDynamicScriptElementLoaded(FindFrameByName("five")));
  tester.ExpectBucketCount(kSubresourceFilterPromptHistogram, true, 1);
  // Check that bubble is shown for new navigation.
  ui_test_utils::NavigateToURL(browser(), url);
  tester.ExpectBucketCount(kSubresourceFilterPromptHistogram, true, 2);
}

// Tests checking how histograms are recorded. ---------------------------------

namespace {

void ExpectHistogramsAreRecordedForTestFrameSet(
    const base::HistogramTester& tester,
    bool expect_performance_measurements) {
  const bool time_recorded =
      expect_performance_measurements && ScopedThreadTimers::IsSupported();

  // The following histograms are generated on the browser side.
  tester.ExpectTotalCount(kEvaluationTotalWallDurationForPage, time_recorded);
  tester.ExpectTotalCount(kEvaluationTotalCPUDurationForPage, time_recorded);

  // The rest is produced by renderers, therefore needs to be merged here.
  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  tester.ExpectTotalCount(kEvaluationTotalWallDurationForDocument,
                          time_recorded ? 6 : 0);
  tester.ExpectTotalCount(kEvaluationTotalCPUDurationForDocument,
                          time_recorded ? 6 : 0);

  tester.ExpectTotalCount(kEvaluationWallDuration, time_recorded ? 7 : 0);
  tester.ExpectTotalCount(kEvaluationCPUDuration, time_recorded ? 7 : 0);

  // Activation timing histograms are always recorded.
  tester.ExpectTotalCount(kActivationWallDuration, 6);
  tester.ExpectTotalCount(kActivationCPUDuration, 6);

  tester.ExpectUniqueSample(
      kDocumentLoadActivationState,
      static_cast<base::Histogram::Sample>(ActivationState::ENABLED), 6);

  EXPECT_THAT(tester.GetAllSamples(kSubresourceLoadsTotal),
              ::testing::ElementsAre(base::Bucket(0, 3), base::Bucket(2, 3)));
  EXPECT_THAT(tester.GetAllSamples(kSubresourceLoadsEvaluated),
              ::testing::ElementsAre(base::Bucket(0, 3), base::Bucket(2, 3)));

  EXPECT_THAT(tester.GetAllSamples(kSubresourceLoadsMatchedRules),
              ::testing::ElementsAre(base::Bucket(0, 4), base::Bucket(2, 2)));
  EXPECT_THAT(tester.GetAllSamples(kSubresourceLoadsDisallowed),
              ::testing::ElementsAre(base::Bucket(0, 4), base::Bucket(2, 2)));
}

}  // namespace

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       ExpectHistogramsAreRecorded) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  const GURL url = GetTestUrl(kTestFrameSetPath);

  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);

  ExpectHistogramsAreRecordedForTestFrameSet(
      tester, false /* expect_performance_measurements */);
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterWithPerformanceMeasurementBrowserTest,
                       ExpectHistogramsAreRecorded) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  const GURL url = GetTestUrl(kTestFrameSetPath);

  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);

  ExpectHistogramsAreRecordedForTestFrameSet(
      tester, true /* expect_performance_measurements */);
}

}  // namespace subresource_filter
