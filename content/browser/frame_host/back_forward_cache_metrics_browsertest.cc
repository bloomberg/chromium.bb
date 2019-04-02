// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

ukm::SourceId ToSourceId(int64_t navigation_id) {
  return ukm::ConvertToSourceId(navigation_id,
                                ukm::SourceIdType::NAVIGATION_ID);
}

using UkmMetrics = std::map<std::string, int64_t>;
using UkmEntry = std::pair<ukm::SourceId, UkmMetrics>;

std::vector<UkmEntry> GetEntries(ukm::TestUkmRecorder* recorder,
                                 std::string entry_name,
                                 const std::vector<std::string>& metrics) {
  std::vector<UkmEntry> results;
  for (const ukm::mojom::UkmEntry* entry :
       recorder->GetEntriesByName(entry_name)) {
    UkmEntry result;
    result.first = entry->source_id;
    for (const std::string& metric_name : metrics) {
      const int64_t* metric_value =
          ukm::TestUkmRecorder::GetEntryMetric(entry, metric_name);
      EXPECT_TRUE(metric_value) << "Metric " << metric_name
                                << " is not found in entry " << entry_name;
      result.second[metric_name] = *metric_value;
    }
    results.push_back(std::move(result));
  }
  return results;
}

std::vector<UkmMetrics> GetMetrics(ukm::TestUkmRecorder* recorder,
                                   std::string entry_name,
                                   const std::vector<std::string>& metrics) {
  std::vector<UkmMetrics> result;
  for (const auto& entry : GetEntries(recorder, entry_name, metrics)) {
    result.push_back(entry.second);
  }
  return result;
}

}  // namespace

class BackForwardCacheMetricsBrowserTest : public ContentBrowserTest,
                                           public WebContentsObserver {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    WebContentsObserver::Observe(shell()->web_contents());
  }

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    navigation_ids_.push_back(navigation_handle->GetNavigationId());
  }

 protected:
  std::vector<int64_t> navigation_ids_;
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheMetricsBrowserTest, UKM) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  const GURL url2(embedded_test_server()->GetURL("/title1.html"));
  const char kChildFrameId[] = "child0";

  EXPECT_TRUE(NavigateToURL(shell(), url2));
  // The navigation entries are:
  // [*url2].

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  // The navigation entries are:
  // [url2, *url1(subframe)].

  EXPECT_TRUE(
      NavigateIframeToURL(shell()->web_contents(), kChildFrameId, url2));
  // The navigation entries are:
  // [url2, url1(subframe), *url1(url2)].

  EXPECT_TRUE(NavigateToURL(shell(), url2));
  // The navigation entries are:
  // [url2, url1(subframe), url1(url2), *url2].

  {
    // We are waiting for two navigations here: main frame and subframe.
    TestNavigationObserver navigation_observer(shell()->web_contents(), 2);
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [url2, url1(subframe), *url1(url2), url2].

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [url2, *url1(subframe), url1(url2), url2].

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [*url2, url1(subframe), url1(url2), url2].

  // There are five new navigations and four back navigations.
  // Navigations 1, 2, 5 are main frame. Navigation 3 is an initial navigation
  // in the subframe, navigation 4 is a subframe navigation.
  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(9));
  ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  // ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);
  // ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);
  // ukm::SourceId id5 = ToSourceId(navigation_ids_[4]);
  ukm::SourceId id6 = ToSourceId(navigation_ids_[5]);
  // ukm::SourceId id7 = ToSourceId(navigation_ids_[6]);
  // ukm::SourceId id8 = ToSourceId(navigation_ids_[7]);
  ukm::SourceId id9 = ToSourceId(navigation_ids_[8]);

  std::string last_navigation_id = "LastCommittedSourceIdForTheSameDocument";

  // First back-forward navigation (#6) navigates back to navigation #3, but it
  // is the subframe navigation, so the corresponding main frame navigation is
  // #2. Navigation #7 loads the subframe for navigation #6.
  // Second back-forward navigation (#8) navigates back to navigation #2,
  // but it is subframe navigation and not reflected here. Third back-forward
  // navigation (#9) navigates back to navigation #1.
  EXPECT_THAT(GetEntries(&recorder, "HistoryNavigation", {last_navigation_id}),
              testing::ElementsAre(UkmEntry{id6, {{last_navigation_id, id2}}},
                                   UkmEntry{id9, {{last_navigation_id, id1}}}));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheMetricsBrowserTest,
                       NavigatedToTheMostRecentEntry) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  const GURL url2(embedded_test_server()->GetURL("/title1.html"));
  const char kChildFrameId[] = "child0";

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(
      NavigateIframeToURL(shell()->web_contents(), kChildFrameId, url2));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  {
    // We are waiting for two navigations here: main frame and subframe.
    TestNavigationObserver navigation_observer(shell()->web_contents(), 2);
    shell()->GoBackOrForward(-2);
    navigation_observer.WaitForNavigationFinished();
  }

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(1);
    navigation_observer.WaitForNavigationFinished();
  }

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [url1(subframe), url1(url2), *url2].

  std::string navigated_to_last_entry =
      "NavigatedToTheMostRecentEntryForDocument";

  // The first back navigation goes to the url1(subframe) entry, while the last
  // active entry for that document was url1(url2).
  // The second back/forward navigation is a subframe one and should be ignored.
  // The last one navigates to the actual entry.
  EXPECT_THAT(
      GetMetrics(&recorder, "HistoryNavigation", {navigated_to_last_entry}),
      testing::ElementsAre(UkmMetrics{{navigated_to_last_entry, false}},
                           UkmMetrics{{navigated_to_last_entry, true}}));
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheMetricsBrowserTest, CloneAndGoBack) {
  ukm::TestAutoSetUkmRecorder recorder;

  const GURL url1(embedded_test_server()->GetURL("/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  // Clone the tab and load the page.
  std::unique_ptr<WebContents> new_tab = shell()->web_contents()->Clone();
  WebContentsImpl* new_tab_impl = static_cast<WebContentsImpl*>(new_tab.get());
  WebContentsObserver::Observe(new_tab.get());
  NavigationController& new_controller = new_tab_impl->GetController();
  {
    TestNavigationObserver clone_observer(new_tab.get());
    new_controller.LoadIfNecessary();
    clone_observer.Wait();
  }

  {
    TestNavigationObserver navigation_observer(new_tab.get());
    new_controller.GoBack();
    navigation_observer.WaitForNavigationFinished();
  }

  {
    TestNavigationObserver navigation_observer(new_tab.get());
    new_controller.GoForward();
    navigation_observer.WaitForNavigationFinished();
  }

  {
    TestNavigationObserver navigation_observer(new_tab.get());
    new_controller.GoBack();
    navigation_observer.WaitForNavigationFinished();
  }

  ASSERT_EQ(navigation_ids_.size(), static_cast<size_t>(6));
  // ukm::SourceId id1 = ToSourceId(navigation_ids_[0]);
  // ukm::SourceId id2 = ToSourceId(navigation_ids_[1]);
  ukm::SourceId id3 = ToSourceId(navigation_ids_[2]);
  ukm::SourceId id4 = ToSourceId(navigation_ids_[3]);
  ukm::SourceId id5 = ToSourceId(navigation_ids_[4]);
  ukm::SourceId id6 = ToSourceId(navigation_ids_[5]);

  std::string last_navigation_id = "LastCommittedSourceIdForTheSameDocument";

  // First two new navigations happen in the original tab.
  // The third navigation reloads the tab for the cloned WebContents.
  // The fourth goes back, but the metrics are not recorded due to it being
  // cloned and the metrics objects missing.
  // The last two navigations, however, should have metrics.
  EXPECT_THAT(GetEntries(&recorder, "HistoryNavigation", {last_navigation_id}),
              testing::ElementsAre(UkmEntry{id5, {{last_navigation_id, id3}}},
                                   UkmEntry{id6, {{last_navigation_id, id4}}}));
}

}  // namespace content
