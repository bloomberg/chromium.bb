// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/pattern.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/page_load_metrics/observers/subresource_filter_metrics_observer.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_database_helper.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/subresource_filter/test_ruleset_publisher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/db/v4_test_util.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter_test_utils.h"
#include "components/subresource_filter/content/browser/content_ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_level.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "components/subresource_filter/core/common/scoped_timers.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

namespace proto = url_pattern_index::proto;

// The path to a multi-frame document used for tests.
static constexpr const char kTestFrameSetPath[] =
    "/subresource_filter/frame_set.html";

// Names of DocumentLoad histograms.
constexpr const char kDocumentLoadActivationLevel[] =
    "SubresourceFilter.DocumentLoad.ActivationState";
constexpr const char kSubresourceLoadsTotal[] =
    "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Total";
constexpr const char kSubresourceLoadsEvaluated[] =
    "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Evaluated";
constexpr const char kSubresourceLoadsMatchedRules[] =
    "SubresourceFilter.DocumentLoad.NumSubresourceLoads.MatchedRules";
constexpr const char kSubresourceLoadsDisallowed[] =
    "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Disallowed";

constexpr const char kSubresourceLoadsTotalForPage[] =
    "SubresourceFilter.PageLoad.NumSubresourceLoads.Total";
constexpr const char kSubresourceLoadsEvaluatedForPage[] =
    "SubresourceFilter.PageLoad.NumSubresourceLoads.Evaluated";
constexpr const char kSubresourceLoadsMatchedRulesForPage[] =
    "SubresourceFilter.PageLoad.NumSubresourceLoads.MatchedRules";
constexpr const char kSubresourceLoadsDisallowedForPage[] =
    "SubresourceFilter.PageLoad.NumSubresourceLoads.Disallowed";

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

// Names of navigation chain patterns histogram.
const char kMatchesPatternHistogramName[] =
    "SubresourceFilter.PageLoad.ActivationList";
const char kNavigationChainSize[] =
    "SubresourceFilter.PageLoad.RedirectChainLength";
const char kSubresourceFilterOnlySuffix[] = ".SubresourceFilterOnly";

// Other histograms.
const char kSubresourceFilterActionsHistogram[] = "SubresourceFilter.Actions";

GURL GetURLWithFragment(const GURL& url, base::StringPiece fragment) {
  GURL::Replacements replacements;
  replacements.SetRefStr(fragment);
  return url.ReplaceComponents(replacements);
}

}  // namespace

namespace subresource_filter {

using subresource_filter::testing::TestRulesetCreator;
using subresource_filter::testing::TestRulesetPair;

// SubresourceFilterDisabledBrowserTest ---------------------------------------

class SubresourceFilterDisabledByDefaultBrowserTest
    : public InProcessBrowserTest {
 public:
  SubresourceFilterDisabledByDefaultBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        "suppress-enabling-subresource-filter-from-fieldtrial-testing-config");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterDisabledByDefaultBrowserTest);
};

// Tests -----------------------------------------------------------------------

// The RulesetService should not even be instantiated when the feature is
// disabled, which should be the default state unless there is an override
// specified in the field trial configuration.
IN_PROC_BROWSER_TEST_F(SubresourceFilterDisabledByDefaultBrowserTest,
                       RulesetServiceNotCreated) {
  EXPECT_FALSE(g_browser_process->subresource_filter_ruleset_service());
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterListInsertingBrowserTest,
                       MainFrameActivation_SubresourceFilterList) {
  content::ConsoleObserverDelegate console_observer(web_contents(),
                                                    kActivationConsoleMessage);
  web_contents()->SetDelegate(&console_observer);
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsSubresourceFilterOnlyURL(url);
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));

  Configuration config(subresource_filter::ActivationLevel::ENABLED,
                       subresource_filter::ActivationScope::ACTIVATION_LIST,
                       subresource_filter::ActivationList::SUBRESOURCE_FILTER);
  ResetConfiguration(std::move(config));

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  EXPECT_EQ(kActivationConsoleMessage, console_observer.message());

  // The main frame document should never be filtered.
  SetRulesetToDisallowURLsWithPathSuffix("frame_with_included_script.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterListInsertingBrowserTest,
                       MainFrameActivationWithWarning_BetterAdsList) {
  content::ConsoleObserverDelegate console_observer1(web_contents(),
                                                     "*show ads*");
  web_contents()->SetDelegate(&console_observer1);
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureURLWithWarning(url,
                          {safe_browsing::SubresourceFilterType::BETTER_ADS});
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));

  Configuration config(subresource_filter::ActivationLevel::ENABLED,
                       subresource_filter::ActivationScope::ACTIVATION_LIST,
                       subresource_filter::ActivationList::BETTER_ADS);
  ResetConfiguration(std::move(config));

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
  EXPECT_EQ(kActivationWarningConsoleMessage, console_observer1.message());

  content::ConsoleObserverDelegate console_observer2(web_contents(),
                                                     "*show ads*");
  web_contents()->SetDelegate(&console_observer2);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  EXPECT_EQ(kActivationWarningConsoleMessage, console_observer2.message());
}

IN_PROC_BROWSER_TEST_F(
    SubresourceFilterListInsertingBrowserTest,
    ExpectRedirectPatternHistogramsAreRecordedForSubresourceFilterOnlyMatch) {
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));

  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsSubresourceFilterOnlyURL(url);

  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);

  tester.ExpectUniqueSample(
      kMatchesPatternHistogramName,
      static_cast<int>(ActivationList::SUBRESOURCE_FILTER), 1);
  EXPECT_THAT(tester.GetAllSamples(std::string(kNavigationChainSize) +
                                   std::string(kSubresourceFilterOnlySuffix)),
              ::testing::ElementsAre(base::Bucket(1, 1)));
}

IN_PROC_BROWSER_TEST_F(
    SubresourceFilterListInsertingBrowserTest,
    ExpectRedirectPatternHistogramsAreRecordedForSubresourceFilterOnlyRedirectMatch) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  const std::string initial_host("a.com");
  const std::string redirected_host("b.com");

  GURL redirect_url(embedded_test_server()->GetURL(
      redirected_host, "/subresource_filter/frame_with_included_script.html"));
  GURL url(embedded_test_server()->GetURL(
      initial_host, "/server-redirect?" + redirect_url.spec()));

  ConfigureAsSubresourceFilterOnlyURL(url.GetOrigin());
  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);
  tester.ExpectUniqueSample(kMatchesPatternHistogramName,
                            static_cast<int>(ActivationList::NONE), 1);
}

// Normally, the subresource filter list is only sync'd in chrome branded
// builds.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       SubresourceFilterListNeedsBranding) {
  bool has_list = database_helper()->HasListSynced(
      safe_browsing::GetUrlSubresourceFilterId());
#if defined(GOOGLE_CHROME_BUILD)
  EXPECT_TRUE(has_list);
#else
  EXPECT_FALSE(has_list);
#endif
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, MainFrameActivation) {
  content::ConsoleObserverDelegate console_observer(web_contents(),
                                                    kActivationConsoleMessage);
  web_contents()->SetDelegate(&console_observer);
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  EXPECT_EQ(console_observer.message(), kActivationConsoleMessage);

  // The main frame document should never be filtered.
  SetRulesetToDisallowURLsWithPathSuffix("frame_with_included_script.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

// There should be no document-level de-/reactivation happening on the renderer
// side as a result of a same document navigation.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       DocumentActivationOutlivesSameDocumentNavigation) {
  GURL url(GetTestUrl("subresource_filter/frame_with_delayed_script.html"));
  ConfigureAsPhishingURL(url);
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
  NavigateFromRendererSide(GetURLWithFragment(url, "ref"));
  EXPECT_FALSE(IsDynamicScriptElementLoaded(web_contents()->GetMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, SubFrameActivation) {
  std::ostringstream message_filter;
  message_filter << kDisallowSubframeConsoleMessagePrefix << "*";
  content::ConsoleObserverDelegate console_observer(web_contents(),
                                                    message_filter.str());
  web_contents()->SetDelegate(&console_observer);

  GURL url(GetTestUrl(kTestFrameSetPath));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);

  const std::vector<const char*> kSubframeNames{"one", "two", "three"};
  const std::vector<bool> kExpectScriptInFrameToLoad{false, true, false};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoad));

  tester.ExpectBucketCount(kSubresourceFilterActionsHistogram, kActionUIShown,
                           1);

  // Console message for subframe blocking should be displayed.
  std::ostringstream result;
  result << kDisallowSubframeConsoleMessagePrefix << "*included_script.js*";
  EXPECT_TRUE(base::MatchPattern(console_observer.message(), result.str()));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       ActivationDisabled_NoConsoleMessage) {
  std::ostringstream message_filter;
  message_filter << kDisallowSubframeConsoleMessageSuffix << "*";
  content::ConsoleObserverDelegate console_observer(web_contents(),
                                                    message_filter.str());
  web_contents()->SetDelegate(&console_observer);

  Configuration config(
      subresource_filter::ActivationLevel::DISABLED,
      subresource_filter::ActivationScope::ACTIVATION_LIST,
      subresource_filter::ActivationList::PHISHING_INTERSTITIAL);
  ResetConfiguration(std::move(config));

  GURL url(GetTestUrl(kTestFrameSetPath));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  // Console message for subframe blocking should not be displayed as filtering
  // is disabled.
  EXPECT_TRUE(console_observer.message().empty());
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       ActivationDryRun_NoConsoleMessage) {
  std::ostringstream message_filter;
  message_filter << kDisallowSubframeConsoleMessageSuffix << "*";
  content::ConsoleObserverDelegate console_observer(web_contents(),
                                                    message_filter.str());
  web_contents()->SetDelegate(&console_observer);

  Configuration config(
      subresource_filter::ActivationLevel::DRYRUN,
      subresource_filter::ActivationScope::ACTIVATION_LIST,
      subresource_filter::ActivationList::PHISHING_INTERSTITIAL);
  ResetConfiguration(std::move(config));

  GURL url(GetTestUrl(kTestFrameSetPath));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  // Console message for subframe blocking should not be displayed as filtering
  // is enabled in dryrun mode.
  EXPECT_TRUE(console_observer.message().empty());
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       SubframeDocumentLoadFiltering) {
  base::HistogramTester histogram_tester;
  GURL url(GetTestUrl(kTestFrameSetPath));
  ConfigureAsPhishingURL(url);

  // Disallow loading subframe documents that in turn would end up loading
  // included_script.js, unless the document is loaded from a whitelisted
  // domain. This enables the third part of this test disallowing a load only
  // after the first redirect.
  const char kWhitelistedDomain[] = "whitelisted.com";
  proto::UrlRule rule = testing::CreateSuffixRule("included_script.html");
  proto::UrlRule whitelist_rule = testing::CreateSuffixRule(kWhitelistedDomain);
  whitelist_rule.set_anchor_right(proto::ANCHOR_TYPE_NONE);
  whitelist_rule.set_semantics(proto::RULE_SEMANTICS_WHITELIST);
  ASSERT_NO_FATAL_FAILURE(SetRulesetWithRules({rule, whitelist_rule}));

  ui_test_utils::NavigateToURL(browser(), url);

  const std::vector<const char*> kSubframeNames{"one", "two", "three"};
  const std::vector<bool> kExpectOnlySecondSubframe{false, true, false};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);
  histogram_tester.ExpectBucketCount(kSubresourceFilterActionsHistogram,
                                     kActionUIShown, 1);

  // Now navigate the first subframe to an allowed URL and ensure that the load
  // successfully commits and the frame gets restored (no longer collapsed).
  GURL allowed_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_allowed_script.html"));
  NavigateFrame(kSubframeNames[0], allowed_subdocument_url);

  const std::vector<bool> kExpectFirstAndSecondSubframe{true, true, false};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectFirstAndSecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectFirstAndSecondSubframe);

  // Navigate the first subframe to a document that does not load the probe JS.
  GURL allowed_empty_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_no_subresources.html"));
  NavigateFrame(kSubframeNames[0], allowed_empty_subdocument_url);

  // Finally, navigate the first subframe to an allowed URL that redirects to a
  // disallowed URL, and verify that:
  //  -- The navigation gets blocked and the frame collapsed (with PlzNavigate).
  //  -- The navigation is cancelled, but the frame is not collapsed (without
  //  PlzNavigate, where BLOCK_REQUEST_AND_COLLAPSE is not supported).
  GURL disallowed_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));
  GURL redirect_to_disallowed_subdocument_url(embedded_test_server()->GetURL(
      kWhitelistedDomain,
      "/server-redirect?" + disallowed_subdocument_url.spec()));
  NavigateFrame(kSubframeNames[0], redirect_to_disallowed_subdocument_url);

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));

  content::RenderFrameHost* frame = FindFrameByName(kSubframeNames[0]);
  ASSERT_TRUE(frame);
  if (content::IsBrowserSideNavigationEnabled()) {
    EXPECT_EQ(disallowed_subdocument_url, frame->GetLastCommittedURL());
    ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);
  } else {
    EXPECT_EQ(allowed_empty_subdocument_url, frame->GetLastCommittedURL());
    ExpectFramesIncludedInLayout(kSubframeNames, kExpectFirstAndSecondSubframe);
  }
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       HistoryNavigationActivation) {
  content::ConsoleObserverDelegate console_observer(web_contents(),
                                                    kActivationConsoleMessage);
  web_contents()->SetDelegate(&console_observer);
  GURL url_with_activation(GetTestUrl(kTestFrameSetPath));
  GURL url_without_activation(
      embedded_test_server()->GetURL("a.com", kTestFrameSetPath));
  ConfigureAsPhishingURL(url_with_activation);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  const std::vector<const char*> kSubframeNames{"one", "two", "three"};
  const std::vector<bool> kExpectScriptInFrameToLoadWithoutActivation{
      true, true, true};
  const std::vector<bool> kExpectScriptInFrameToLoadWithActivation{false, true,
                                                                   false};

  ui_test_utils::NavigateToURL(browser(), url_without_activation);
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoadWithoutActivation));

  // No message should be displayed for navigating to URL without activation.
  EXPECT_TRUE(console_observer.message().empty());

  ui_test_utils::NavigateToURL(browser(), url_with_activation);
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoadWithActivation));

  // Console message should now be displayed.
  EXPECT_EQ(console_observer.message(), kActivationConsoleMessage);

  ASSERT_TRUE(web_contents()->GetController().CanGoBack());
  content::WindowedNotificationObserver back_navigation_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  web_contents()->GetController().GoBack();
  back_navigation_stop_observer.Wait();
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoadWithoutActivation));

  ASSERT_TRUE(web_contents()->GetController().CanGoForward());
  content::WindowedNotificationObserver forward_navigation_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  web_contents()->GetController().GoForward();
  forward_navigation_stop_observer.Wait();
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoadWithActivation));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       FailedProvisionalLoadInMainframe) {
  GURL url_with_activation_but_dns_error(
      "http://host-with-dns-lookup-failure/");
  GURL url_with_activation_but_not_existent(GetTestUrl("non-existent.html"));
  GURL url_without_activation(GetTestUrl(kTestFrameSetPath));

  ConfigureAsPhishingURL(url_with_activation_but_dns_error);
  ConfigureAsPhishingURL(url_with_activation_but_not_existent);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  const std::vector<const char*> kSubframeNames{"one", "two", "three"};
  const std::vector<bool> kExpectScriptInFrameToLoad{true, true, true};

  for (const auto& url_with_activation :
       {url_with_activation_but_dns_error,
        url_with_activation_but_not_existent}) {
    SCOPED_TRACE(url_with_activation);

    // In either test case, there is no server-supplied error page, so Chrome's
    // own navigation error page is shown. This also triggers a background
    // request to load navigation corrections (aka. Link Doctor), and once the
    // results are back, there is a navigation to a second error page with the
    // suggestions. Hence the wait for two navigations in a row.
    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(), url_with_activation, 2);
    ui_test_utils::NavigateToURL(browser(), url_without_activation);
    ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
        kSubframeNames, kExpectScriptInFrameToLoad));
  }
}

// The page-level activation state on the browser-side should not be reset when
// a same document navigation starts in the main frame. Verify this by
// dynamically inserting a subframe afterwards, and still expecting activation.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PageLevelActivationOutlivesSameDocumentNavigation) {
  content::ConsoleObserverDelegate console_observer(web_contents(),
                                                    kActivationConsoleMessage);
  web_contents()->SetDelegate(&console_observer);
  GURL url(GetTestUrl(kTestFrameSetPath));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  content::RenderFrameHost* frame = FindFrameByName("one");
  ASSERT_TRUE(frame);
  EXPECT_FALSE(WasParsedScriptElementLoaded(frame));

  NavigateFromRendererSide(GetURLWithFragment(url, "ref"));

  ASSERT_NO_FATAL_FAILURE(InsertDynamicFrameWithScript());
  content::RenderFrameHost* dynamic_frame = FindFrameByName("dynamic");
  ASSERT_TRUE(dynamic_frame);
  EXPECT_FALSE(WasParsedScriptElementLoaded(dynamic_frame));

  EXPECT_EQ(console_observer.message(), kActivationConsoleMessage);
}

// If a navigation starts but aborts before commit, page level activation should
// remain unchanged.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PageLevelActivationOutlivesAbortedNavigation) {
  GURL url(GetTestUrl(kTestFrameSetPath));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  content::RenderFrameHost* frame = FindFrameByName("one");
  EXPECT_FALSE(WasParsedScriptElementLoaded(frame));

  // Start a new navigation, but abort it right away.
  GURL aborted_url = GURL("https://abort-me.com");
  content::TestNavigationManager manager(
      browser()->tab_strip_model()->GetActiveWebContents(), aborted_url);

  chrome::NavigateParams params(browser(), aborted_url,
                                ui::PAGE_TRANSITION_LINK);
  chrome::Navigate(&params);
  ASSERT_TRUE(manager.WaitForRequestStart());
  browser()->tab_strip_model()->GetActiveWebContents()->Stop();

  // Will return false if the navigation was successfully aborted.
  ASSERT_FALSE(manager.WaitForResponse());
  manager.WaitForNavigationFinished();

  // Now, dynamically insert a frame and expect that it is still activated.
  ASSERT_NO_FATAL_FAILURE(InsertDynamicFrameWithScript());
  content::RenderFrameHost* dynamic_frame = FindFrameByName("dynamic");
  ASSERT_TRUE(dynamic_frame);
  EXPECT_FALSE(WasParsedScriptElementLoaded(dynamic_frame));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, DynamicFrame) {
  GURL url(GetTestUrl("subresource_filter/frame_set.html"));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_NO_FATAL_FAILURE(InsertDynamicFrameWithScript());
  content::RenderFrameHost* dynamic_frame = FindFrameByName("dynamic");
  ASSERT_TRUE(dynamic_frame);
  EXPECT_FALSE(WasParsedScriptElementLoaded(dynamic_frame));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       RulesetVerified_Activation) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ContentRulesetService* service =
      g_browser_process->subresource_filter_ruleset_service();
  ASSERT_TRUE(service->ruleset_dealer());
  auto ruleset_handle =
      base::MakeUnique<VerifiedRuleset::Handle>(service->ruleset_dealer());
  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("https://example.com/"), ActivationLevel::ENABLED, false);

  testing::TestActivationStateCallbackReceiver receiver;
  AsyncDocumentSubresourceFilter filter(ruleset_handle.get(), std::move(params),
                                        receiver.GetCallback());
  receiver.WaitForActivationDecision();
  receiver.ExpectReceivedOnce(ActivationState(ActivationLevel::ENABLED));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, NoRuleset_NoActivation) {
  // Do not set the ruleset, which results in an invalid ruleset.
  ContentRulesetService* service =
      g_browser_process->subresource_filter_ruleset_service();
  ASSERT_TRUE(service->ruleset_dealer());
  auto ruleset_handle =
      base::MakeUnique<VerifiedRuleset::Handle>(service->ruleset_dealer());
  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("https://example.com/"), ActivationLevel::ENABLED, false);

  testing::TestActivationStateCallbackReceiver receiver;
  AsyncDocumentSubresourceFilter filter(ruleset_handle.get(), std::move(params),
                                        receiver.GetCallback());
  receiver.WaitForActivationDecision();
  receiver.ExpectReceivedOnce(ActivationState(ActivationLevel::DISABLED));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       InvalidRuleset_NoActivation) {
  const char kTestRulesetSuffix[] = "foo";
  const int kNumberOfRules = 500;
  TestRulesetCreator ruleset_creator;
  TestRulesetPair test_ruleset_pair;
  ASSERT_NO_FATAL_FAILURE(
      ruleset_creator.CreateRulesetToDisallowURLsWithManySuffixes(
          kTestRulesetSuffix, kNumberOfRules, &test_ruleset_pair));
  testing::TestRuleset::CorruptByTruncating(test_ruleset_pair.indexed, 123);

  // Just publish the corrupt indexed file directly, to simulate it being
  // corrupt on startup.
  ContentRulesetService* service =
      g_browser_process->subresource_filter_ruleset_service();
  ASSERT_TRUE(service->ruleset_dealer());
  service->PublishNewRulesetVersion(
      testing::TestRuleset::Open(test_ruleset_pair.indexed));

  auto ruleset_handle =
      base::MakeUnique<VerifiedRuleset::Handle>(service->ruleset_dealer());
  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("https://example.com/"), ActivationLevel::ENABLED, false);

  testing::TestActivationStateCallbackReceiver receiver;
  AsyncDocumentSubresourceFilter filter(ruleset_handle.get(), std::move(params),
                                        receiver.GetCallback());
  receiver.WaitForActivationDecision();
  receiver.ExpectReceivedOnce(ActivationState(ActivationLevel::DISABLED));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, PageLoadMetrics) {
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  content::ConsoleObserverDelegate console_observer(web_contents(),
                                                    kActivationConsoleMessage);
  web_contents()->SetDelegate(&console_observer);

  // Force a navigation to another page, which flushes page load metrics for the
  // previous page load.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  histogram_tester.ExpectTotalCount(internal::kHistogramSubresourceFilterCount,
                                    1);

  // No message should be displayed for about blank URL.
  EXPECT_TRUE(console_observer.message().empty());
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PRE_MainFrameActivationOnStartup) {
  SetRulesetToDisallowURLsWithPathSuffix("included_script.js");
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       MainFrameActivationOnStartup) {
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);
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
  GURL a_url(embedded_test_server()->GetURL(
      "a.com", "/subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);
  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);
  tester.ExpectBucketCount(kSubresourceFilterActionsHistogram, kActionUIShown,
                           1);
  // Check that the bubble is not shown again for this navigation.
  EXPECT_FALSE(IsDynamicScriptElementLoaded(FindFrameByName("five")));
  tester.ExpectBucketCount(kSubresourceFilterActionsHistogram, kActionUIShown,
                           1);
  // Check that bubble is shown for new navigation. Must be cross site to avoid
  // triggering smart UI on Android.
  ConfigureAsPhishingURL(a_url);
  ui_test_utils::NavigateToURL(browser(), a_url);
  tester.ExpectBucketCount(kSubresourceFilterActionsHistogram, kActionUIShown,
                           2);
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       CrossSiteSubFrameActivationWithoutWhitelist) {
  GURL a_url(embedded_test_server()->GetURL(
      "a.com", "/subresource_filter/frame_cross_site_set.html"));
  ConfigureAsPhishingURL(a_url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), a_url);
  ExpectParsedScriptElementLoadedStatusInFrames(
      std::vector<const char*>{"b", "c", "d"}, {false, false, false});
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       CrossSiteSubFrameActivationWithWhitelist) {
  GURL a_url(embedded_test_server()->GetURL(
      "a.com", "/subresource_filter/frame_cross_site_set.html"));
  ConfigureAsPhishingURL(a_url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetWithRules({testing::CreateSuffixRule("included_script.js"),
                           testing::CreateWhitelistRuleForDocument("c.com")}));
  ui_test_utils::NavigateToURL(browser(), a_url);
  ExpectParsedScriptElementLoadedStatusInFrames(
      std::vector<const char*>{"b", "d"}, {false, true});
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       RendererDebugURL_NoLeakedThrottlePtrs) {
  // We have checks in the throttle manager that we don't improperly leak
  // activation state throttles. It would be nice to test things directly but it
  // isn't very feasible right now without exposing a bunch of internal guts of
  // the throttle manager.
  //
  // This test should crash the *browser process* with CHECK failures if the
  // component is faulty. The CHECK assumes that the crash URL and other
  // renderer debug URLs do not create a navigation throttle. See
  // crbug.com/736658.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
      content::NotificationService::AllSources());
  browser()->OpenURL(content::OpenURLParams(
      GURL(content::kChromeUICrashURL), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
  observer.Wait();
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       DisableRuleset_SubresourceAllowed) {
  Configuration config = Configuration::MakePresetForLiveRunOnPhishingSites();
  config.activation_options.should_disable_ruleset_rules = true;
  ResetConfiguration(std::move(config));

  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

// Tests checking how histograms are recorded. ---------------------------------

namespace {

void ExpectHistogramsAreRecordedForTestFrameSet(
    const base::HistogramTester& tester,
    bool expect_performance_measurements) {
  const bool time_recorded =
      expect_performance_measurements && ScopedThreadTimers::IsSupported();

  // The following histograms are generated on the browser side.
  tester.ExpectUniqueSample(kSubresourceLoadsTotalForPage, 6, 1);
  tester.ExpectUniqueSample(kSubresourceLoadsEvaluatedForPage, 6, 1);
  tester.ExpectUniqueSample(kSubresourceLoadsMatchedRulesForPage, 4, 1);
  tester.ExpectUniqueSample(kSubresourceLoadsDisallowedForPage, 4, 1);
  tester.ExpectTotalCount(kEvaluationTotalWallDurationForPage, time_recorded);
  tester.ExpectTotalCount(kEvaluationTotalCPUDurationForPage, time_recorded);

  // The rest is produced by renderers, therefore needs to be merged here.
  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  tester.ExpectTotalCount(kEvaluationTotalWallDurationForDocument,
                          time_recorded ? 6 : 0);
  tester.ExpectTotalCount(kEvaluationTotalCPUDurationForDocument,
                          time_recorded ? 6 : 0);

  // 5 subframes, each with an include.js, plus a top level include.js.
  int num_subresource_checks = 5 + 5 + 1;
  tester.ExpectTotalCount(kEvaluationWallDuration,
                          time_recorded ? num_subresource_checks : 0);
  tester.ExpectTotalCount(kEvaluationCPUDuration,
                          time_recorded ? num_subresource_checks : 0);

  // Activation WallDuration histogram is always recorded.
  tester.ExpectTotalCount(kActivationWallDuration, 6);

  // Activation CPUDuration histogram is recorded only if base::ThreadTicks is
  // supported.
  tester.ExpectTotalCount(kActivationCPUDuration,
                          ScopedThreadTimers::IsSupported() ? 6 : 0);

  tester.ExpectUniqueSample(
      kDocumentLoadActivationLevel,
      static_cast<base::Histogram::Sample>(ActivationLevel::ENABLED), 6);

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
                       ExpectHistogramsAreRecordedForFilteredChildFrames) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  // Navigate to a URL which doesn't include any filtered resources in the
  // top-level document, but which includes filtered resources in child frames.
  const GURL url = GetTestUrl(kTestFrameSetPath);
  ConfigureAsPhishingURL(url);

  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);

  ExpectHistogramsAreRecordedForTestFrameSet(
      tester, false /* expect_performance_measurements */);

  // Force a navigation to another page, which flushes page load metrics for the
  // previous page load.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Make sure that pages that have subresource filtered in child frames are
  // also counted.
  tester.ExpectTotalCount(internal::kHistogramSubresourceFilterCount, 1);
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       ExpectPerformanceHistogramsAreRecorded) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ResetConfigurationToEnableOnPhishingSites(
      true /* measure_performance */, false /* whitelist_site_on_reload */);
  const GURL url = GetTestUrl(kTestFrameSetPath);
  ConfigureAsPhishingURL(url);

  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);

  ExpectHistogramsAreRecordedForTestFrameSet(
      tester, true /* expect_performance_measurements */);
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       ExpectHistogramsNotRecordedWhenFilteringNotActivated) {
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));
  const GURL url = GetTestUrl(kTestFrameSetPath);
  // Note: The |url| is not configured to be fishing.

  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);

  // The following histograms are generated only when filtering is activated.
  tester.ExpectTotalCount(kSubresourceLoadsTotalForPage, 0);
  tester.ExpectTotalCount(kSubresourceLoadsEvaluatedForPage, 0);
  tester.ExpectTotalCount(kSubresourceLoadsMatchedRulesForPage, 0);
  tester.ExpectTotalCount(kSubresourceLoadsDisallowedForPage, 0);
  tester.ExpectTotalCount(kEvaluationTotalWallDurationForPage, 0);
  tester.ExpectTotalCount(kEvaluationTotalCPUDurationForPage, 0);

  // The rest is produced by renderers, therefore needs to be merged here.
  content::FetchHistogramsFromChildProcesses();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // But they still should not be recorded as the filtering is not activated.
  tester.ExpectTotalCount(kEvaluationTotalWallDurationForDocument, 0);
  tester.ExpectTotalCount(kEvaluationTotalCPUDurationForDocument, 0);
  tester.ExpectTotalCount(kEvaluationWallDuration, 0);
  tester.ExpectTotalCount(kEvaluationCPUDuration, 0);

  tester.ExpectTotalCount(kActivationWallDuration, 0);
  tester.ExpectTotalCount(kActivationCPUDuration, 0);

  tester.ExpectTotalCount(kSubresourceLoadsTotal, 0);
  tester.ExpectTotalCount(kSubresourceLoadsEvaluated, 0);
  tester.ExpectTotalCount(kSubresourceLoadsMatchedRules, 0);
  tester.ExpectTotalCount(kSubresourceLoadsDisallowed, 0);

  // Although SubresourceFilterAgents still record the activation decision.
  tester.ExpectUniqueSample(
      kDocumentLoadActivationLevel,
      static_cast<base::Histogram::Sample>(ActivationLevel::DISABLED), 6);
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       ActivationEnabledOnReloadByDefault) {
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      content::MessageLoopRunner::QuitMode::DEFERRED);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  tester.ExpectTotalCount(
      internal::kHistogramSubresourceFilterActivationDecision, 2);
  tester.ExpectBucketCount(
      internal::kHistogramSubresourceFilterActivationDecision,
      static_cast<int>(ActivationDecision::ACTIVATED), 2);

  tester.ExpectTotalCount(
      internal::kHistogramSubresourceFilterActivationDecisionReload, 1);
  tester.ExpectBucketCount(
      internal::kHistogramSubresourceFilterActivationDecisionReload,
      static_cast<int>(ActivationDecision::ACTIVATED), 1);
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       WhitelistSiteOnReload_ActivationDisabledOnReload) {
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ResetConfigurationToEnableOnPhishingSites(
      false /* measure_performance */, true /* whitelist_site_on_reload */);
  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      content::MessageLoopRunner::QuitMode::DEFERRED);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  tester.ExpectTotalCount(
      internal::kHistogramSubresourceFilterActivationDecision, 2);
  tester.ExpectBucketCount(
      internal::kHistogramSubresourceFilterActivationDecision,
      static_cast<int>(ActivationDecision::ACTIVATED), 1);
  tester.ExpectBucketCount(
      internal::kHistogramSubresourceFilterActivationDecision,
      static_cast<int>(ActivationDecision::URL_WHITELISTED), 1);

  tester.ExpectTotalCount(
      internal::kHistogramSubresourceFilterActivationDecisionReload, 1);
  tester.ExpectBucketCount(
      internal::kHistogramSubresourceFilterActivationDecisionReload,
      static_cast<int>(ActivationDecision::URL_WHITELISTED), 1);
}

IN_PROC_BROWSER_TEST_F(
    SubresourceFilterBrowserTest,
    WhitelistSiteOnReload_ActivationDisabledOnReloadFromScript) {
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ResetConfigurationToEnableOnPhishingSites(
      false /* measure_performance */, true /* whitelist_site_on_reload */);
  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      content::MessageLoopRunner::QuitMode::DEFERRED);
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "location.reload();"));
  observer.Wait();
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  tester.ExpectTotalCount(
      internal::kHistogramSubresourceFilterActivationDecision, 2);
  tester.ExpectBucketCount(
      internal::kHistogramSubresourceFilterActivationDecision,
      static_cast<int>(ActivationDecision::ACTIVATED), 1);
  tester.ExpectBucketCount(
      internal::kHistogramSubresourceFilterActivationDecision,
      static_cast<int>(ActivationDecision::URL_WHITELISTED), 1);

  tester.ExpectTotalCount(
      internal::kHistogramSubresourceFilterActivationDecisionReload, 1);
  tester.ExpectBucketCount(
      internal::kHistogramSubresourceFilterActivationDecisionReload,
      static_cast<int>(ActivationDecision::URL_WHITELISTED), 1);
}

IN_PROC_BROWSER_TEST_F(
    SubresourceFilterBrowserTest,
    WhitelistSiteOnReload_ActivationDisabledOnNavigationToSameURL) {
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ResetConfigurationToEnableOnPhishingSites(
      false /* measure_performance */, true /* whitelist_site_on_reload */);
  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      content::MessageLoopRunner::QuitMode::DEFERRED);
  std::string nav_frame_script = "location.href = '" + url.spec() + "';";
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(), nav_frame_script));
  observer.Wait();
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  tester.ExpectTotalCount(
      internal::kHistogramSubresourceFilterActivationDecision, 2);
  tester.ExpectBucketCount(
      internal::kHistogramSubresourceFilterActivationDecision,
      static_cast<int>(ActivationDecision::ACTIVATED), 1);
  tester.ExpectBucketCount(
      internal::kHistogramSubresourceFilterActivationDecision,
      static_cast<int>(ActivationDecision::URL_WHITELISTED), 1);

  tester.ExpectTotalCount(
      internal::kHistogramSubresourceFilterActivationDecisionReload, 1);
  tester.ExpectBucketCount(
      internal::kHistogramSubresourceFilterActivationDecisionReload,
      static_cast<int>(ActivationDecision::URL_WHITELISTED), 1);
}

}  // namespace subresource_filter
