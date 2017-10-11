// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_database_helper.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

}  // namespace

SubresourceFilterBrowserTest::SubresourceFilterBrowserTest() {}
SubresourceFilterBrowserTest::~SubresourceFilterBrowserTest() {}

void SubresourceFilterBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                  base::JoinString(RequiredFeatures(), ","));
}

std::vector<base::StringPiece> SubresourceFilterBrowserTest::RequiredFeatures()
    const {
  return {kSafeBrowsingSubresourceFilter.name, "SafeBrowsingV4OnlyEnabled",
          kSafeBrowsingSubresourceFilterExperimentalUI.name};
}

void SubresourceFilterBrowserTest::SetUp() {
  database_helper_ = CreateTestDatabase();
  InProcessBrowserTest::SetUp();
}

void SubresourceFilterBrowserTest::TearDown() {
  InProcessBrowserTest::TearDown();
  // Unregister test factories after InProcessBrowserTest::TearDown
  // (which destructs SafeBrowsingService).
  database_helper_.reset();
}

void SubresourceFilterBrowserTest::SetUpOnMainThread() {
  base::FilePath test_data_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
  host_resolver()->AddSimulatedFailure("host-with-dns-lookup-failure");

  host_resolver()->AddRule("*", "127.0.0.1");
  content::SetupCrossSiteRedirector(embedded_test_server());

  // Add content/test/data for cross_site_iframe_factory.html
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");

  ASSERT_TRUE(embedded_test_server()->Start());
  ResetConfigurationToEnableOnPhishingSites();

  settings_manager_ = SubresourceFilterProfileContextFactory::GetForProfile(
                          browser()->profile())
                          ->settings_manager();
#if defined(OS_ANDROID)
  EXPECT_TRUE(settings_manager->should_use_smart_ui());
#endif
}

std::unique_ptr<TestSafeBrowsingDatabaseHelper>
SubresourceFilterBrowserTest::CreateTestDatabase() {
  return base::MakeUnique<TestSafeBrowsingDatabaseHelper>();
}

GURL SubresourceFilterBrowserTest::GetTestUrl(
    const std::string& relative_url) const {
  return embedded_test_server()->base_url().Resolve(relative_url);
}

void SubresourceFilterBrowserTest::ConfigureAsPhishingURL(const GURL& url) {
  safe_browsing::ThreatMetadata metadata;
  database_helper_->MarkUrlAsMatchingListIdWithMetadata(
      url, safe_browsing::GetUrlSocEngId(), metadata);
}

void SubresourceFilterBrowserTest::ConfigureAsSubresourceFilterOnlyURL(
    const GURL& url) {
  safe_browsing::ThreatMetadata metadata;
  database_helper_->MarkUrlAsMatchingListIdWithMetadata(
      url, safe_browsing::GetUrlSubresourceFilterId(), metadata);
}

void SubresourceFilterBrowserTest::ConfigureURLWithWarning(
    const GURL& url,
    std::vector<safe_browsing::SubresourceFilterType> filter_types) {
  safe_browsing::ThreatMetadata metadata;

  for (auto type : filter_types) {
    metadata.subresource_filter_match[type] =
        safe_browsing::SubresourceFilterLevel::WARN;
  }
  database_helper_->MarkUrlAsMatchingListIdWithMetadata(
      url, safe_browsing::GetUrlSubresourceFilterId(), metadata);
}

content::WebContents* SubresourceFilterBrowserTest::web_contents() const {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

content::RenderFrameHost* SubresourceFilterBrowserTest::FindFrameByName(
    const std::string& name) const {
  return content::FrameMatchingPredicate(
      web_contents(), base::Bind(&content::FrameMatchesName, name));
}

bool SubresourceFilterBrowserTest::WasParsedScriptElementLoaded(
    content::RenderFrameHost* rfh) {
  DCHECK(rfh);
  bool script_resource_was_loaded = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      rfh, "domAutomationController.send(!!document.scriptExecuted)",
      &script_resource_was_loaded));
  return script_resource_was_loaded;
}

void SubresourceFilterBrowserTest::
    ExpectParsedScriptElementLoadedStatusInFrames(
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

void SubresourceFilterBrowserTest::ExpectFramesIncludedInLayout(
    const std::vector<const char*>& frame_names,
    const std::vector<bool>& expect_displayed) {
  const char kScript[] =
      "window.domAutomationController.send("
      "  document.getElementsByName(\"%s\")[0].clientWidth"
      ");";

  ASSERT_EQ(expect_displayed.size(), frame_names.size());
  for (size_t i = 0; i < frame_names.size(); ++i) {
    SCOPED_TRACE(frame_names[i]);
    int client_width = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        web_contents()->GetMainFrame(),
        base::StringPrintf(kScript, frame_names[i]), &client_width));
    EXPECT_EQ(expect_displayed[i], !!client_width) << client_width;
  }
}

bool SubresourceFilterBrowserTest::IsDynamicScriptElementLoaded(
    content::RenderFrameHost* rfh) {
  DCHECK(rfh);
  bool script_resource_was_loaded = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      rfh, "insertScriptElementAndReportSuccess()",
      &script_resource_was_loaded));
  return script_resource_was_loaded;
}

void SubresourceFilterBrowserTest::InsertDynamicFrameWithScript() {
  bool frame_was_loaded = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents()->GetMainFrame(), "insertFrameWithScriptAndNotify()",
      &frame_was_loaded));
  ASSERT_TRUE(frame_was_loaded);
}

void SubresourceFilterBrowserTest::NavigateFromRendererSide(const GURL& url) {
  content::TestNavigationObserver navigation_observer(web_contents(), 1);
  ASSERT_TRUE(content::ExecuteScript(
      web_contents()->GetMainFrame(),
      base::StringPrintf("window.location = \"%s\";", url.spec().c_str())));
  navigation_observer.Wait();
}

void SubresourceFilterBrowserTest::NavigateFrame(const char* frame_name,
                                                 const GURL& url) {
  content::TestNavigationObserver navigation_observer(web_contents(), 1);
  ASSERT_TRUE(content::ExecuteScript(
      web_contents()->GetMainFrame(),
      base::StringPrintf("document.getElementsByName(\"%s\")[0].src = \"%s\";",
                         frame_name, url.spec().c_str())));
  navigation_observer.Wait();
}

void SubresourceFilterBrowserTest::SetRulesetToDisallowURLsWithPathSuffix(
    const std::string& suffix) {
  TestRulesetPair test_ruleset_pair;
  ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
      suffix, &test_ruleset_pair);
  ASSERT_NO_FATAL_FAILURE(
      test_ruleset_publisher_.SetRuleset(test_ruleset_pair.unindexed));
}

void SubresourceFilterBrowserTest::SetRulesetWithRules(
    const std::vector<proto::UrlRule>& rules) {
  TestRulesetPair test_ruleset_pair;
  ruleset_creator_.CreateRulesetWithRules(rules, &test_ruleset_pair);
  ASSERT_NO_FATAL_FAILURE(
      test_ruleset_publisher_.SetRuleset(test_ruleset_pair.unindexed));
}

void SubresourceFilterBrowserTest::ResetConfiguration(Configuration config) {
  scoped_configuration_.ResetConfiguration(std::move(config));
}

void SubresourceFilterBrowserTest::ResetConfigurationToEnableOnPhishingSites(
    bool measure_performance,
    bool whitelist_site_on_reload) {
  Configuration config = Configuration::MakePresetForLiveRunOnPhishingSites();
  config.activation_options.performance_measurement_rate =
      measure_performance ? 1.0 : 0.0;
  config.activation_options.should_whitelist_site_on_reload =
      whitelist_site_on_reload;
  ResetConfiguration(std::move(config));
}

std::unique_ptr<TestSafeBrowsingDatabaseHelper>
SubresourceFilterListInsertingBrowserTest::CreateTestDatabase() {
  std::vector<safe_browsing::ListIdentifier> list_ids = {
      safe_browsing::GetUrlSubresourceFilterId()};
  return base::MakeUnique<TestSafeBrowsingDatabaseHelper>(std::move(list_ids));
}

}  // namespace subresource_filter
