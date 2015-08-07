// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_details.h"

#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/metrics/metrics_memory_details.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::Bucket;
using testing::ContainerEq;
using testing::ElementsAre;

namespace {

class TestMemoryDetails : public MetricsMemoryDetails {
 public:
  TestMemoryDetails()
      : MetricsMemoryDetails(base::Bind(&base::DoNothing), nullptr) {}

  void StartFetchAndWait() {
    uma_.reset(new base::HistogramTester());
    StartFetch(FROM_CHROME_ONLY);
    content::RunMessageLoop();
  }

  // Returns a HistogramTester which observed the most recent call to
  // StartFetchAndWait().
  base::HistogramTester* uma() { return uma_.get(); }

 private:
  ~TestMemoryDetails() override {}

  void OnDetailsAvailable() override {
    MetricsMemoryDetails::OnDetailsAvailable();
    // Exit the loop initiated by StartFetchAndWait().
    base::MessageLoop::current()->Quit();
  }

  scoped_ptr<base::HistogramTester> uma_;

  DISALLOW_COPY_AND_ASSIGN(TestMemoryDetails);
};

}  // namespace

class SiteDetailsBrowserTest : public InProcessBrowserTest {
 public:
  SiteDetailsBrowserTest() {}
  ~SiteDetailsBrowserTest() override {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data so we can use cross_site_iframe_factory.html
    base::FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir.AppendASCII("content/test/data/"));
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  }

  // The expected number of non-renderer processes, which is platform dependent.
  int other_process_count() const {
#if defined(OS_LINUX)
    return 3;  // Browser, GPU, Zygote.
#else
    return 2;  // Browser, GPU.
#endif
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SiteDetailsBrowserTest);
};

// Test the accuracy of SiteDetails process estimation, in the presence of
// multiple iframes, navigation, multiple BrowsingInstances, and multiple tabs
// in the same BrowsingInstance.
IN_PROC_BROWSER_TEST_F(SiteDetailsBrowserTest, ManyCrossSiteIframes) {
  // Page with 14 nested oopifs across 9 sites (a.com through i.com).
  // None of these are https.
  GURL abcdefghi_url = embedded_test_server()->GetURL(
      "a.com",
      "/cross_site_iframe_factory.html?a(b(a(b,c,d,e,f,g,h)),c,d,e,i(f))");
  ui_test_utils::NavigateToURL(browser(), abcdefghi_url);

  // Get the metrics.
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(9, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(9, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(9, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesTotalProcessCountEstimate"),
              ElementsAre(Bucket(9 + other_process_count(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountNoLimit"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesTotalProcessCountEstimate"),
              ElementsAre(Bucket(1 + other_process_count(), 1)));

  // Navigate to a different, disjoint set of 7 sites.
  GURL pqrstuv_url = embedded_test_server()->GetURL(
      "p.com",
      "/cross_site_iframe_factory.html?p(q(r),r(s),s(t),t(q),u(u),v(p))");
  ui_test_utils::NavigateToURL(browser(), pqrstuv_url);

  details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(7, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(7, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(7, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesTotalProcessCountEstimate"),
              ElementsAre(Bucket(7 + other_process_count(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountEstimate"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountNoLimit"),
              ElementsAre(Bucket(1, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesTotalProcessCountEstimate"),
              ElementsAre(Bucket(1 + other_process_count(), 1)));

  // Open a second tab (different BrowsingInstance) with 4 sites (a through d).
  GURL abcd_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c(d())))");
  AddTabAtIndex(1, abcd_url, ui::PAGE_TRANSITION_TYPED);

  details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(11, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(11, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(11, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesTotalProcessCountEstimate"),
              ElementsAre(Bucket(11 + other_process_count(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountEstimate"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));  // TODO(nick): This should be 2.
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountNoLimit"),
              ElementsAre(Bucket(2, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesTotalProcessCountEstimate"),
              ElementsAre(Bucket(2 + other_process_count(), 1)));

  // Open a third tab (different BrowsingInstance) with the same 4 sites.
  AddTabAtIndex(2, abcd_url, ui::PAGE_TRANSITION_TYPED);

  details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(3, 1)));
  // Could be 11 if subframe processes were reused across BrowsingInstances.
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(15, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(11, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(15, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesTotalProcessCountEstimate"),
              ElementsAre(Bucket(15 + other_process_count(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));  // TODO(nick): This should be 3.
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountNoLimit"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesTotalProcessCountEstimate"),
              ElementsAre(Bucket(3 + other_process_count(), 1)));

  // Do a window.open() to obtain a fourth tab in the same BrowsingInstance as
  // the third tab. The new page uses the same four sites "a-d" as third tab,
  // plus an additional site "e". The estimated process counts should increase
  // by one (not five) from the previous result, since the new tab can reuse the
  // four processes already in the BrowsingInstance.
  GURL dcbae_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?d(c(b(a(e))))");
  ui_test_utils::UrlLoadObserver load_complete(
      dcbae_url, content::NotificationService::AllSources());
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.open('" + dcbae_url.spec() + "');"));
  ASSERT_EQ(4, browser()->tab_strip_model()->count());
  load_complete.Wait();

  details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              ElementsAre(Bucket(3, 1)));
  // Could be 11 if subframe processes were reused across BrowsingInstances.
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountEstimate"),
              ElementsAre(Bucket(16, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountLowerBound"),
              ElementsAre(Bucket(12, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesProcessCountNoLimit"),
              ElementsAre(Bucket(16, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateAllSitesTotalProcessCountEstimate"),
              ElementsAre(Bucket(16 + other_process_count(), 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountEstimate"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountLowerBound"),
              ElementsAre(Bucket(1, 1)));  // TODO(nick): This should be 3.
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesProcessCountNoLimit"),
              ElementsAre(Bucket(3, 1)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.IsolateHttpsSitesTotalProcessCountEstimate"),
              ElementsAre(Bucket(3 + other_process_count(), 1)));
}
