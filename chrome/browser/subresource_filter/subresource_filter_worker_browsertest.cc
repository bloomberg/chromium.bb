// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

enum class OffMainThreadFetchPolicy {
  kEnabled,
  kDisabled,
};

class SubresourceFilterWorkerFetchBrowserTest
    : public SubresourceFilterBrowserTest,
      public ::testing::WithParamInterface<OffMainThreadFetchPolicy> {
 public:
  SubresourceFilterWorkerFetchBrowserTest() {}
  ~SubresourceFilterWorkerFetchBrowserTest() override {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::vector<base::StringPiece> features =
        SubresourceFilterBrowserTest::RequiredFeatures();
    if (GetParam() == OffMainThreadFetchPolicy::kEnabled) {
      features.push_back(features::kOffMainThreadFetch.name);
    } else {
      command_line->AppendSwitchASCII(switches::kDisableFeatures,
                                      features::kOffMainThreadFetch.name);
    }
    command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                    base::JoinString(features, ","));
  }

  void ClearTitle() {
    ASSERT_TRUE(content::ExecuteScript(web_contents()->GetMainFrame(),
                                       "document.title = \"\";"));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterWorkerFetchBrowserTest);
};

IN_PROC_BROWSER_TEST_P(SubresourceFilterWorkerFetchBrowserTest, WorkerFetch) {
  const base::string16 fetch_succeeded_title =
      base::ASCIIToUTF16("FetchSucceeded");
  const base::string16 fetch_failed_title = base::ASCIIToUTF16("FetchFailed");
  GURL url(GetTestUrl("subresource_filter/worker_fetch.html"));
  ConfigureAsPhishingURL(url);

  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));
  {
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(),
        fetch_succeeded_title);
    title_watcher.AlsoWaitForTitle(fetch_failed_title);
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(fetch_succeeded_title, title_watcher.WaitAndGetTitle());
  }
  ClearTitle();
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("worker_fetch_data.txt"));
  {
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(),
        fetch_succeeded_title);
    title_watcher.AlsoWaitForTitle(fetch_failed_title);
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(fetch_failed_title, title_watcher.WaitAndGetTitle());
  }
  ClearTitle();
  // The main frame document should never be filtered.
  SetRulesetToDisallowURLsWithPathSuffix("worker_fetch.html");
  {
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(),
        fetch_succeeded_title);
    title_watcher.AlsoWaitForTitle(fetch_failed_title);
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(fetch_succeeded_title, title_watcher.WaitAndGetTitle());
  }
}

INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        SubresourceFilterWorkerFetchBrowserTest,
                        ::testing::Values(OffMainThreadFetchPolicy::kEnabled,
                                          OffMainThreadFetchPolicy::kDisabled));

}  // namespace subresource_filter
