// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/subresource_filter/test_ruleset_publisher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

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

class SubresourceFilterBrowserTest : public InProcessBrowserTest {
 public:
  SubresourceFilterBrowserTest() {}
  ~SubresourceFilterBrowserTest() override {}

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
        kActivationScopeAllSites));
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

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterBrowserTest);
};

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
  GURL url(GetTestUrl("subresource_filter/frame_set.html"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);

  const char* kSubframeNames[] = {"one", "two"};
  for (const char* subframe_name : kSubframeNames) {
    content::RenderFrameHost* frame = FindFrameByName(subframe_name);
    ASSERT_TRUE(frame);
    EXPECT_FALSE(WasParsedScriptElementLoaded(frame));
  }
  tester.ExpectBucketCount(kSubresourceFilterPromptHistogram, true, 1);
}

// The page-level activation state on the browser-side should not be reset when
// a same-page navigation starts in the main frame. Verify this by dynamically
// inserting a subframe afterwards, and still expecting activation.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PageLevelActivationOutlivesSamePageNavigation) {
  GURL url(GetTestUrl("subresource_filter/frame_set.html"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ui_test_utils::NavigateToURL(browser(), url);

  ui_test_utils::NavigateToURL(browser(), GetURLWithFragment(url, "ref"));

  ASSERT_NO_FATAL_FAILURE(InsertDynamicFrameWithScript());
  content::RenderFrameHost* dynamic_frame = FindFrameByName("dynamic");
  ASSERT_TRUE(dynamic_frame);
  EXPECT_FALSE(WasParsedScriptElementLoaded(dynamic_frame));
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
  GURL url(GetTestUrl("subresource_filter/frame_set.html"));
  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(), url);
  tester.ExpectBucketCount(kSubresourceFilterPromptHistogram, true, 1);
  // Check that the bubble is not shown again for this navigation.
  EXPECT_FALSE(IsDynamicScriptElementLoaded(FindFrameByName("three")));
  tester.ExpectBucketCount(kSubresourceFilterPromptHistogram, true, 1);
  // Check that bubble is shown for new navigation.
  ui_test_utils::NavigateToURL(browser(), url);
  tester.ExpectBucketCount(kSubresourceFilterPromptHistogram, true, 2);
}

}  // namespace subresource_filter
