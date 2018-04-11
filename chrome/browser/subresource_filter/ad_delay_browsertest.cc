// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_filter/content/common/ad_delay_throttle.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class AdDelayBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  AdDelayBrowserTest() : subresource_filter::SubresourceFilterBrowserTest() {}
  ~AdDelayBrowserTest() override {}

  void SetUp() override {
    scoped_features_.InitAndEnableFeatureWithParameters(
        subresource_filter::kDelayUnsafeAds,
        {{subresource_filter::kInsecureDelayParam, DelayParamToUse()}});
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  // Calls fetchResources on the page, which issues fetch()es for two resources.
  //
  // Returns the name of the file whose fetch complete event triggered the
  // callback.
  // - If |policy| is kBothFetches, the returned file will be the last file
  //   loaded.
  // - If |policy| is kOneFetch, the returned file will be the first file
  //   loaded.
  enum class WaitForPolicy { kOneFetch, kBothFetches };
  std::string FetchResources(WaitForPolicy policy) {
    std::string ret;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::StringPrintf(
            "fetchResources(%s);",
            policy == WaitForPolicy::kBothFetches ? "true" : "false"),
        &ret));
    return ret;
  }

  base::TimeDelta GetExpectedDelay() const {
    return base::TimeDelta::FromMilliseconds(
        base::GetFieldTrialParamByFeatureAsInt(
            subresource_filter::kDelayUnsafeAds,
            subresource_filter::kInsecureDelayParam,
            subresource_filter::AdDelayThrottle::kDefaultDelay
                .InMilliseconds()));
  }

 protected:
  // 10 minutes.
  virtual std::string DelayParamToUse() { return "600000"; }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

class MinimalAdDelayBrowserTest : public AdDelayBrowserTest {
  std::string DelayParamToUse() override { return "100"; }
};

IN_PROC_BROWSER_TEST_F(AdDelayBrowserTest, NoAd_NoDelay) {
  GURL url(embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_multiple_fetches.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  base::ElapsedTimer timer;
  FetchResources(WaitForPolicy::kBothFetches);
  EXPECT_GT(GetExpectedDelay(), timer.Elapsed());
}

IN_PROC_BROWSER_TEST_F(AdDelayBrowserTest, AdRequest_IsDelayed) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL url(embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_multiple_fetches.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  // If the included_script.js is delayed, assert that the
  // included_allowed_script will always load before it. Because the allowed
  // script is fetched second, this is a reasonably strong statement.
  base::ElapsedTimer timer;
  std::string first_resource = FetchResources(WaitForPolicy::kOneFetch);
  EXPECT_GE(GetExpectedDelay(), timer.Elapsed());
  EXPECT_EQ("included_allowed_script.js", first_resource);
}

IN_PROC_BROWSER_TEST_F(MinimalAdDelayBrowserTest, AdRequest_IsNotBlackholed) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL url(embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_multiple_fetches.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  // The test succeeds if it does not time out.
  base::ElapsedTimer timer;
  FetchResources(WaitForPolicy::kBothFetches);
  EXPECT_LE(GetExpectedDelay(), timer.Elapsed());
}

IN_PROC_BROWSER_TEST_F(MinimalAdDelayBrowserTest, SyncXHRAd) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  std::string script = R"(
    var request = new XMLHttpRequest();
    request.open('GET', '/subresource_filter/included_script.js', false);
    request.send(null);
  )";
  base::ElapsedTimer timer;
  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(), script.c_str()));
  subresource_filter::AdDelayThrottle::Factory factory;
  EXPECT_LE(factory.insecure_delay(), timer.Elapsed());
}
