// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/page_load_metrics/observers/subresource_filter_metrics_observer.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/subresource_filter/test_ruleset_publisher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "components/safe_browsing_db/util.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/activation_level.h"
#include "components/subresource_filter/core/common/scoped_timers.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// The path to a multi-frame document used for tests.
static constexpr const char kTestFrameSetPath[] =
    "subresource_filter/frame_set.html";

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

// Other histograms.
const char kSubresourceFilterPromptHistogram[] =
    "SubresourceFilter.Prompt.NumVisibility";

// Database manager that allows any URL to be configured as blacklisted for
// testing.
class FakeSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  FakeSafeBrowsingDatabaseManager() {}

  void AddBlacklistedURL(const GURL& url,
                         safe_browsing::SBThreatType threat_type) {
    url_to_threat_type_[url] = threat_type;
  }

 protected:
  ~FakeSafeBrowsingDatabaseManager() override {}

  bool CheckBrowseUrl(const GURL& url, Client* client) override {
    if (!url_to_threat_type_.count(url))
      return true;

    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&Client::OnCheckBrowseUrlResult, base::Unretained(client),
                   url, url_to_threat_type_[url],
                   safe_browsing::ThreatMetadata()));
    return false;
  }

  bool CheckResourceUrl(const GURL& url, Client* client) override {
    return true;
  }

  bool IsSupported() const override { return true; }
  bool ChecksAreAlwaysAsync() const override { return false; }
  bool CanCheckResourceType(
      content::ResourceType /* resource_type */) const override {
    return true;
  }

  safe_browsing::ThreatSource GetThreatSource() const override {
    return safe_browsing::ThreatSource::LOCAL_PVER4;
  }

  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override {
    return true;
  }

 private:
  std::map<GURL, safe_browsing::SBThreatType> url_to_threat_type_;

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingDatabaseManager);
};

// UI manager that never actually shows any interstitials, but emulates as if
// the user chose to proceed through them.
class FakeSafeBrowsingUIManager
    : public safe_browsing::TestSafeBrowsingUIManager {
 public:
  FakeSafeBrowsingUIManager() {}

 protected:
  ~FakeSafeBrowsingUIManager() override {}

  void DisplayBlockingPage(const UnsafeResource& resource) override {
    resource.callback_thread->PostTask(
        FROM_HERE, base::Bind(resource.callback, true /* proceed */));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingUIManager);
};

GURL GetURLWithFragment(const GURL& url, base::StringPiece fragment) {
  GURL::Replacements replacements;
  replacements.SetRefStr(fragment);
  return url.ReplaceComponents(replacements);
}

GURL GetURLWithQuery(const GURL& url, base::StringPiece query) {
  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

}  // namespace

namespace subresource_filter {

using subresource_filter::testing::ScopedSubresourceFilterFeatureToggle;
using subresource_filter::testing::TestRulesetPublisher;
using subresource_filter::testing::TestRulesetCreator;
using subresource_filter::testing::TestRulesetPair;
using ActivationDecision =
    ContentSubresourceFilterDriverFactory::ActivationDecision;

// SubresourceFilterDisabledBrowserTest ---------------------------------------

using SubresourceFilterDisabledBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SubresourceFilterDisabledBrowserTest,
                       RulesetServiceNotCreatedByDefault) {
  EXPECT_FALSE(g_browser_process->subresource_filter_ruleset_service());
}

// SubresourceFilterBrowserTest -----------------------------------------------

class SubresourceFilterBrowserTestImpl : public InProcessBrowserTest {
 public:
  explicit SubresourceFilterBrowserTestImpl(bool measure_performance,
                                            bool whitelist_site_on_reload)
      : measure_performance_(measure_performance),
        whitelist_site_on_reload_(whitelist_site_on_reload) {}

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

  void SetUpInProcessBrowserTestFixture() override {
    fake_safe_browsing_database_ = new FakeSafeBrowsingDatabaseManager();
    test_safe_browsing_factory_.SetTestDatabaseManager(
        fake_safe_browsing_database_.get());
    test_safe_browsing_factory_.SetTestUIManager(new FakeSafeBrowsingUIManager);
    safe_browsing::SafeBrowsingService::RegisterFactory(
        &test_safe_browsing_factory_);
  }

  void SetUpOnMainThread() override {
    scoped_feature_toggle_.reset(new ScopedSubresourceFilterFeatureToggle(
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationLevelEnabled,
        kActivationScopeActivationList, kActivationListPhishingInterstitial,
        measure_performance_ ? "1" : "0", "" /* suppress_notifications */,
        whitelist_site_on_reload_ ? "true" : "false"));

    base::FilePath test_data_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    host_resolver()->AddSimulatedFailure("host-with-dns-lookup-failure");
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetTestUrl(const std::string& relative_url) {
    return embedded_test_server()->base_url().Resolve(relative_url);
  }

  void ConfigureAsPhishingURL(const GURL& url) {
    fake_safe_browsing_database_->AddBlacklistedURL(
        url, safe_browsing::SB_THREAT_TYPE_URL_PHISHING);
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

  void ExpectParsedScriptElementLoadedStatusInFrames(
      const std::vector<const char*>& frame_names,
      const std::vector<bool>& expect_loaded) {
    ASSERT_EQ(expect_loaded.size(), frame_names.size());
    for (size_t i = 0; i < frame_names.size(); ++i) {
      SCOPED_TRACE(frame_names[i]);
      content::RenderFrameHost* frame = FindFrameByName(frame_names[i]);
      ASSERT_TRUE(frame);
      ASSERT_EQ(expect_loaded[i], WasParsedScriptElementLoaded(frame));
    }
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

  void NavigateFromRendererSide(const GURL& url) {
    ASSERT_TRUE(content::ExecuteScript(
        web_contents()->GetMainFrame(),
        base::StringPrintf("window.location = \"%s\";", url.spec().c_str())));
  }

  void SetRulesetToDisallowURLsWithPathSuffix(const std::string& suffix) {
    TestRulesetPair test_ruleset_pair;
    ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
        suffix, &test_ruleset_pair);
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_publisher_.SetRuleset(test_ruleset_pair.unindexed));
  }

  void SetRulesetWithRules(const std::vector<proto::UrlRule>& rules) {
    TestRulesetPair test_ruleset_pair;
    ruleset_creator_.CreateRulesetWithRules(rules, &test_ruleset_pair);
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_publisher_.SetRuleset(test_ruleset_pair.unindexed));
  }

 private:
  TestRulesetCreator ruleset_creator_;
  safe_browsing::TestSafeBrowsingServiceFactory test_safe_browsing_factory_;
  scoped_refptr<FakeSafeBrowsingDatabaseManager> fake_safe_browsing_database_;

  std::unique_ptr<ScopedSubresourceFilterFeatureToggle> scoped_feature_toggle_;
  TestRulesetPublisher test_ruleset_publisher_;
  const bool measure_performance_;
  const bool whitelist_site_on_reload_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterBrowserTestImpl);
};

class SubresourceFilterBrowserTest : public SubresourceFilterBrowserTestImpl {
 public:
  SubresourceFilterBrowserTest()
      : SubresourceFilterBrowserTestImpl(false, false) {}
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
      : SubresourceFilterBrowserTestImpl(true, false) {}
};

class SubresourceFilterWhitelistSiteOnReloadBrowserTest
    : public SubresourceFilterBrowserTestImpl {
 public:
  SubresourceFilterWhitelistSiteOnReloadBrowserTest()
      : SubresourceFilterBrowserTestImpl(false, true) {}
};

enum WebSocketCreationPolicy {
  IN_MAIN_FRAME,
  IN_WORKER,
};
class SubresourceFilterWebSocketBrowserTest
    : public SubresourceFilterBrowserTestImpl,
      public ::testing::WithParamInterface<WebSocketCreationPolicy> {
 public:
  SubresourceFilterWebSocketBrowserTest()
      : SubresourceFilterBrowserTestImpl(false, false) {}

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTestImpl::SetUpOnMainThread();
    websocket_test_server_ = base::MakeUnique<net::SpawnedTestServer>(
        net::SpawnedTestServer::TYPE_WS, net::SpawnedTestServer::kLocalhost,
        net::GetWebSocketTestDataDirectory());
    ASSERT_TRUE(websocket_test_server_->Start());
  }

  net::SpawnedTestServer* websocket_test_server() {
    return websocket_test_server_.get();
  }

  GURL GetWebSocketUrl(const std::string& path) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr("ws");
    return websocket_test_server_->GetURL(path).ReplaceComponents(replacements);
  }

  void CreateWebSocketAndExpectResult(const GURL& url,
                                      bool expect_connection_success) {
    bool websocket_connection_succeeded = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::StringPrintf("connectWebSocket('%s');", url.spec().c_str()),
        &websocket_connection_succeeded));
    EXPECT_EQ(expect_connection_success, websocket_connection_succeeded);
  }

 private:
  std::unique_ptr<net::SpawnedTestServer> websocket_test_server_;
};

// Tests -----------------------------------------------------------------------

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, MainFrameActivation) {
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

  tester.ExpectBucketCount(kSubresourceFilterPromptHistogram, true, 1);
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       HistoryNavigationActivation) {
  GURL url_without_activation(GetTestUrl(kTestFrameSetPath));
  GURL url_with_activation(
      GetURLWithQuery(url_without_activation, "activation"));
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

  ui_test_utils::NavigateToURL(browser(), url_with_activation);
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectScriptInFrameToLoadWithActivation));

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
// a same-page navigation starts in the main frame. Vrrify this by dynamically
// inserting a subframe afterwards, and still expecting activation.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PageLevelActivationOutlivesSamePageNavigation) {
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

// Schemes 'about:' and 'chrome-native:' are loaded synchronously as empty
// documents in Blink, so there is no chance (or need) to send an activation
// IPC message. Make sure that histograms are not polluted with these loads.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, NoActivationOnAboutBlank) {
  GURL url(GetTestUrl("subresource_filter/frame_set_sync_loads.html"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ConfigureAsPhishingURL(url);

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
      kDocumentLoadActivationLevel,
      static_cast<base::Histogram::Sample>(ActivationLevel::ENABLED), 2);
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, PageLoadMetrics) {
  GURL url(GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ConfigureAsPhishingURL(url);

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));

  // Force a navigation to another page, which flushes page load metrics for the
  // previous page load.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  histogram_tester.ExpectTotalCount(internal::kHistogramSubresourceFilterCount,
                                    1);
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
  ConfigureAsPhishingURL(url);
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

IN_PROC_BROWSER_TEST_P(SubresourceFilterWebSocketBrowserTest, BlockWebSocket) {
  GURL url(GetTestUrl(
      base::StringPrintf("subresource_filter/page_with_websocket.html?%s",
                         GetParam() == IN_WORKER ? "inWorker" : "")));
  GURL websocket_url(GetWebSocketUrl("echo-with-no-extension"));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("echo-with-no-extension"));
  ui_test_utils::NavigateToURL(browser(), url);
  CreateWebSocketAndExpectResult(websocket_url,
                                 false /* expect_connection_success */);
}

IN_PROC_BROWSER_TEST_P(SubresourceFilterWebSocketBrowserTest,
                       DoNotBlockWebSocketNoActivatedFrame) {
  GURL url(GetTestUrl(
      base::StringPrintf("subresource_filter/page_with_websocket.html?%s",
                         GetParam() == IN_WORKER ? "inWorker" : "")));
  GURL websocket_url(GetWebSocketUrl("echo-with-no-extension"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("echo-with-no-extension"));
  ui_test_utils::NavigateToURL(browser(), url);

  CreateWebSocketAndExpectResult(websocket_url,
                                 true /* expect_connection_success */);
}

IN_PROC_BROWSER_TEST_P(SubresourceFilterWebSocketBrowserTest,
                       DoNotBlockWebSocketInActivatedFrameWithNoRule) {
  GURL url(GetTestUrl(
      base::StringPrintf("subresource_filter/page_with_websocket.html?%s",
                         GetParam() == IN_WORKER ? "inWorker" : "")));
  GURL websocket_url(GetWebSocketUrl("echo-with-no-extension"));
  ConfigureAsPhishingURL(url);
  ui_test_utils::NavigateToURL(browser(), url);

  CreateWebSocketAndExpectResult(websocket_url,
                                 true /* expect_connection_success */);
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    SubresourceFilterWebSocketBrowserTest,
    ::testing::Values(WebSocketCreationPolicy::IN_WORKER,
                      WebSocketCreationPolicy::IN_MAIN_FRAME));

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

  tester.ExpectTotalCount(kEvaluationWallDuration, time_recorded ? 6 : 0);
  tester.ExpectTotalCount(kEvaluationCPUDuration, time_recorded ? 6 : 0);

  // Activation timing histograms are always recorded.
  tester.ExpectTotalCount(kActivationWallDuration, 6);
  tester.ExpectTotalCount(kActivationCPUDuration, 6);

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

IN_PROC_BROWSER_TEST_F(SubresourceFilterWithPerformanceMeasurementBrowserTest,
                       ExpectHistogramsAreRecorded) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
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

IN_PROC_BROWSER_TEST_F(SubresourceFilterWhitelistSiteOnReloadBrowserTest,
                       ActivationDisabledOnReload) {
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

IN_PROC_BROWSER_TEST_F(SubresourceFilterWhitelistSiteOnReloadBrowserTest,
                       ActivationDisabledOnReloadFromScript) {
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

IN_PROC_BROWSER_TEST_F(SubresourceFilterWhitelistSiteOnReloadBrowserTest,
                       ActivationDisabledOnNavigationToSameURL) {
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
