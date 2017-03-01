// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/page_load_metrics/observers/aborts_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/core_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/document_write_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/no_state_prefetch_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_origin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/page_load_metrics/page_load_metrics_messages.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "net/http/failing_http_transaction_factory.h"
#include "net/http/http_cache.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

// Waits until a PageLoadMetricsMsg_TimingUpdated message IPC is received
// matching a PageLoadTiming. See WaitForMatchingIPC for details.
class TimingUpdatedObserver : public content::BrowserMessageFilter {
 public:
  // A bitvector to express which timing fields to match on.
  enum ExpectedTimingFields {
    FIRST_PAINT = 1 << 0,
    FIRST_CONTENTFUL_PAINT = 1 << 1
  };

  explicit TimingUpdatedObserver(content::RenderWidgetHost* render_widget_host)
      : content::BrowserMessageFilter(PageLoadMetricsMsgStart) {
    render_widget_host->GetProcess()->AddFilter(this);

    // Roundtrip to the IO thread, to ensure that the filter is properly
    // installed.
    content::BrowserThread::PostTaskAndReply(
        content::BrowserThread::IO, FROM_HERE, base::Bind(&base::DoNothing),
        base::Bind(&TimingUpdatedObserver::Quit, this));
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
    run_loop_.reset(nullptr);
  }

  // Add the given timing fields to the set of fields to match on.
  void AddMatchingFields(ExpectedTimingFields fields) {
    matching_fields_ |= fields;
  }

  // Instructs observer to also watch for |count|
  // WebLoadingBehaviorDocumentWriteBlockReload events.
  void MatchDocumentWriteBlockReload(int count) {
    match_document_write_block_reload_ = count;
  }

  // Waits for a TimingUpdated IPC that matches the fields set by
  // |AddMatchingFields|.  All matching fields must be set in a TimingUpdated
  // IPC for it to end this wait.
  void WaitForMatchingIPC() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (matched_timing_update_)
      return;

    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
    run_loop_.reset(nullptr);
  }

 private:
  bool OnMessageReceived(const IPC::Message& message) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    IPC_BEGIN_MESSAGE_MAP(TimingUpdatedObserver, message)
      IPC_MESSAGE_HANDLER(PageLoadMetricsMsg_TimingUpdated, OnTimingUpdated)
    IPC_END_MESSAGE_MAP()

    return false;
  }

  bool OnTimingUpdated(const page_load_metrics::PageLoadTiming& timing,
                       const page_load_metrics::PageLoadMetadata& metadata) {
    if (match_document_write_block_reload_ > 0 &&
        metadata.behavior_flags &
            blink::WebLoadingBehaviorFlag::
                WebLoadingBehaviorDocumentWriteBlockReload) {
      --match_document_write_block_reload_;
    }

    if (match_document_write_block_reload_ > 0) {
      return true;
    }

    if ((!(matching_fields_ & FIRST_PAINT) || timing.first_paint) &&
        (!(matching_fields_ & FIRST_CONTENTFUL_PAINT) ||
         timing.first_contentful_paint)) {
      // Ensure that any other handlers of this message, for example the real
      // PageLoadMetric observers, get a chance to handle this message before
      // this waiter unblocks.
      content::BrowserThread::PostTask(
          content::BrowserThread::IO, FROM_HERE,
          base::Bind(&TimingUpdatedObserver::BounceTimingUpdate, this, timing,
                     metadata));
    }
    return true;
  }

  void BounceTimingUpdate(const page_load_metrics::PageLoadTiming& timing,
                          const page_load_metrics::PageLoadMetadata& metadata) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&TimingUpdatedObserver::SetTimingUpdatedAndQuit, this));
  }

  void Quit() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (run_loop_)
      run_loop_->Quit();
  }

  void SetTimingUpdatedAndQuit() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    matched_timing_update_ = true;
    Quit();
  }

  ~TimingUpdatedObserver() override {}

  std::unique_ptr<base::RunLoop> run_loop_;
  int matching_fields_ = 0;  // A bitvector composed from ExpectedTimingFields.
  bool matched_timing_update_ = false;
  int match_document_write_block_reload_ = 0;
};

}  // namespace

class PageLoadMetricsBrowserTest : public InProcessBrowserTest {
 public:
  PageLoadMetricsBrowserTest() {}
  ~PageLoadMetricsBrowserTest() override {}

 protected:
  void NavigateToUntrackedUrl() {
    ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  }

  bool NoPageLoadMetricsRecorded() {
    return histogram_tester_.GetTotalCountsForPrefix("PageLoad.").empty();
  }

  scoped_refptr<TimingUpdatedObserver> CreateTimingUpdatedObserver() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    scoped_refptr<TimingUpdatedObserver> observer(new TimingUpdatedObserver(
        web_contents->GetRenderViewHost()->GetWidget()));
    return observer;
  }

  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageLoadMetricsBrowserTest);
};

void FailAllNetworkTransactions(net::URLRequestContextGetter* getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  net::HttpCache* cache(
      getter->GetURLRequestContext()->http_transaction_factory()->GetCache());
  DCHECK(cache);
  std::unique_ptr<net::FailingHttpTransactionFactory> factory(
      new net::FailingHttpTransactionFactory(cache->GetSession(),
                                             net::ERR_FAILED));
  // Throw away old version; since this is a browser test, there is no
  // need to restore the old state.
  cache->SetHttpNetworkTransactionFactoryForTesting(std::move(factory));
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NoPageLoadMetricsRecorded());
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NewPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  NavigateToUntrackedUrl();

  histogram_tester_.ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramParseDuration, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramParseBlockedOnScriptLoad, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramParseBlockedOnScriptExecution, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramTotalBytes, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramPageTimingForegroundDuration, 1);

  // Verify that NoPageLoadMetricsRecorded returns false when PageLoad metrics
  // have been recorded.
  EXPECT_FALSE(NoPageLoadMetricsRecorded());
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, SamePageNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html#hash"));
  NavigateToUntrackedUrl();

  histogram_tester_.ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, SameUrlNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  NavigateToUntrackedUrl();

  // We expect one histogram sample for each navigation to title1.html.
  histogram_tester_.ExpectTotalCount(internal::kHistogramDomContentLoaded, 2);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 2);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 2);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NonHtmlMainResource) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/circle.svg"));
  NavigateToUntrackedUrl();
  EXPECT_TRUE(NoPageLoadMetricsRecorded());
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NonHttpOrHttpsUrl) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIVersionURL));
  NavigateToUntrackedUrl();
  EXPECT_TRUE(NoPageLoadMetricsRecorded());
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, HttpErrorPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/page_load_metrics/404.html"));
  NavigateToUntrackedUrl();
  EXPECT_TRUE(NoPageLoadMetricsRecorded());
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, ChromeErrorPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Configure the network stack to fail all attempted loads with a network
  // error, which will cause Chrome to display an error page.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter =
      browser()->profile()->GetRequestContext();
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&FailAllNetworkTransactions,
                 base::RetainedRef(url_request_context_getter)));

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  NavigateToUntrackedUrl();
  EXPECT_TRUE(NoPageLoadMetricsRecorded());
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, Ignore204Pages) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/page204.html"));
  NavigateToUntrackedUrl();
  EXPECT_TRUE(NoPageLoadMetricsRecorded());
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, IgnoreDownloads) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::ScopedTempDir downloads_directory;
  ASSERT_TRUE(downloads_directory.CreateUniqueTempDir());
  browser()->profile()->GetPrefs()->SetFilePath(
      prefs::kDownloadDefaultDirectory, downloads_directory.GetPath());
  content::DownloadTestObserverTerminal downloads_observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()),
      1,  // == wait_count (only waiting for "download-test3.gif").
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/download-test3.gif"));
  downloads_observer.WaitForFinished();

  NavigateToUntrackedUrl();
  EXPECT_TRUE(NoPageLoadMetricsRecorded());
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PreloadDocumentWrite) {
  ASSERT_TRUE(embedded_test_server()->Start());

  scoped_refptr<TimingUpdatedObserver> fcp_observer =
      CreateTimingUpdatedObserver();
  fcp_observer->AddMatchingFields(
      TimingUpdatedObserver::FIRST_CONTENTFUL_PAINT);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_external_script.html"));
  fcp_observer->WaitForMatchingIPC();

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteParseStartToFirstContentfulPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoPreloadDocumentWrite) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_no_script.html"));
  NavigateToUntrackedUrl();

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteParseStartToFirstContentfulPaint, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoDocumentWrite) {
  ASSERT_TRUE(embedded_test_server()->Start());

  scoped_refptr<TimingUpdatedObserver> fcp_observer =
      CreateTimingUpdatedObserver();
  fcp_observer->AddMatchingFields(
      TimingUpdatedObserver::FIRST_CONTENTFUL_PAINT);

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  fcp_observer->WaitForMatchingIPC();

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteParseStartToFirstContentfulPaint, 0);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DocumentWriteBlock) {
  ASSERT_TRUE(embedded_test_server()->Start());

  scoped_refptr<TimingUpdatedObserver> fcp_observer =
      CreateTimingUpdatedObserver();
  fcp_observer->AddMatchingFields(
      TimingUpdatedObserver::FIRST_CONTENTFUL_PAINT);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html"));
  fcp_observer->WaitForMatchingIPC();

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DocumentWriteReload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  scoped_refptr<TimingUpdatedObserver> fcp_observer =
      CreateTimingUpdatedObserver();
  fcp_observer->AddMatchingFields(
      TimingUpdatedObserver::FIRST_CONTENTFUL_PAINT);
  scoped_refptr<TimingUpdatedObserver> reload_observer =
      CreateTimingUpdatedObserver();
  reload_observer->MatchDocumentWriteBlockReload(2);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html"));

  // Reload should not log the histogram as the script is not blocked.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html"));

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html"));

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);

  fcp_observer->WaitForMatchingIPC();
  reload_observer->WaitForMatchingIPC();

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockReloadCount, 2);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DocumentWriteAsync) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_async.html"));
  NavigateToUntrackedUrl();

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DocumentWriteSameDomain) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_external_script.html"));
  NavigateToUntrackedUrl();

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoDocumentWriteScript) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_no_script.html"));
  NavigateToUntrackedUrl();

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, BadXhtml) {
  ASSERT_TRUE(embedded_test_server()->Start());

  scoped_refptr<TimingUpdatedObserver> timing_observer =
      CreateTimingUpdatedObserver();
  timing_observer->AddMatchingFields(TimingUpdatedObserver::FIRST_PAINT);

  // When an XHTML page contains invalid XML, it causes a paint of the error
  // message without a layout. Page load metrics currently treats this as an
  // error. Eventually, we'll fix this by special casing the handling of
  // documents with non-well-formed XML on the blink side. See crbug.com/627607
  // for more.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/page_load_metrics/badxml.xhtml"));

  timing_observer->WaitForMatchingIPC();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstPaint, 0);
  histogram_tester_.ExpectBucketCount(page_load_metrics::internal::kErrorEvents,
                                      page_load_metrics::ERR_BAD_TIMING_IPC, 1);
}

// Test code that aborts provisional navigations.
// TODO(csharrison): Move these to unit tests once the navigation API in content
// properly calls NavigationHandle/NavigationThrottle methods.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, AbortNewNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  chrome::NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  content::TestNavigationManager manager(
      browser()->tab_strip_model()->GetActiveWebContents(), url);

  chrome::Navigate(&params);
  EXPECT_TRUE(manager.WaitForRequestStart());

  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  chrome::NavigateParams params2(browser(), url2,
                                 ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  content::TestNavigationManager manager2(
      browser()->tab_strip_model()->GetActiveWebContents(), url2);
  chrome::Navigate(&params2);

  manager2.WaitForNavigationFinished();
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramAbortNewNavigationBeforeCommit, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, AbortReload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  chrome::NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  content::TestNavigationManager manager(
      browser()->tab_strip_model()->GetActiveWebContents(), url);

  chrome::Navigate(&params);
  EXPECT_TRUE(manager.WaitForRequestStart());

  chrome::NavigateParams params2(browser(), url, ui::PAGE_TRANSITION_RELOAD);
  content::TestNavigationManager manager2(
      browser()->tab_strip_model()->GetActiveWebContents(), url);
  chrome::Navigate(&params2);

  manager2.WaitForNavigationFinished();
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramAbortReloadBeforeCommit, 1);
}

// TODO(crbug.com/675061): Flaky on Win7 dbg.
#if defined(OS_WIN)
#define MAYBE_AbortClose DISABLED_AbortClose
#else
#define MAYBE_AbortClose AbortClose
#endif
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, MAYBE_AbortClose) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  chrome::NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  content::TestNavigationManager manager(
      browser()->tab_strip_model()->GetActiveWebContents(), url);

  chrome::Navigate(&params);
  EXPECT_TRUE(manager.WaitForRequestStart());

  browser()->tab_strip_model()->GetActiveWebContents()->Close();

  manager.WaitForNavigationFinished();

  histogram_tester_.ExpectTotalCount(internal::kHistogramAbortCloseBeforeCommit,
                                     1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, AbortMultiple) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  chrome::NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  content::TestNavigationManager manager(
      browser()->tab_strip_model()->GetActiveWebContents(), url);

  chrome::Navigate(&params);
  EXPECT_TRUE(manager.WaitForRequestStart());

  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  chrome::NavigateParams params2(browser(), url2, ui::PAGE_TRANSITION_TYPED);
  content::TestNavigationManager manager2(
      browser()->tab_strip_model()->GetActiveWebContents(), url2);
  chrome::Navigate(&params2);

  EXPECT_TRUE(manager2.WaitForRequestStart());
  manager.WaitForNavigationFinished();

  GURL url3(embedded_test_server()->GetURL("/title3.html"));
  chrome::NavigateParams params3(browser(), url3, ui::PAGE_TRANSITION_TYPED);
  content::TestNavigationManager manager3(
      browser()->tab_strip_model()->GetActiveWebContents(), url3);
  chrome::Navigate(&params3);

  EXPECT_TRUE(manager3.WaitForRequestStart());
  manager2.WaitForNavigationFinished();

  manager3.WaitForNavigationFinished();

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramAbortNewNavigationBeforeCommit, 2);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       NoAbortMetricsOnClientRedirect) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL first_url(embedded_test_server()->GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), first_url);

  GURL second_url(embedded_test_server()->GetURL("/title2.html"));
  chrome::NavigateParams params(browser(), second_url,
                                ui::PAGE_TRANSITION_LINK);
  content::TestNavigationManager manager(
      browser()->tab_strip_model()->GetActiveWebContents(), second_url);
  chrome::Navigate(&params);
  EXPECT_TRUE(manager.WaitForRequestStart());

  {
    content::TestNavigationManager reload_manager(
        browser()->tab_strip_model()->GetActiveWebContents(), first_url);
    EXPECT_TRUE(content::ExecuteScript(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.location.reload();"));
  }

  manager.WaitForNavigationFinished();

  EXPECT_TRUE(histogram_tester_
                  .GetTotalCountsForPrefix("PageLoad.Experimental.AbortTiming.")
                  .empty());
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       FirstMeaningfulPaintRecorded) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));

  // Wait until the renderer finishes observing layouts.
  const int kNetworkIdleTime = 500;
  const int kMargin = 500;
  const std::string javascript = base::StringPrintf(
      "setTimeout(() => window.domAutomationController.send(true), %d)",
      kNetworkIdleTime + kMargin);
  bool result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      javascript, &result));
  EXPECT_TRUE(result);

  NavigateToUntrackedUrl();
  histogram_tester_.ExpectUniqueSample(
      internal::kHistogramFirstMeaningfulPaintStatus,
      internal::FIRST_MEANINGFUL_PAINT_RECORDED, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramFirstMeaningfulPaint, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramParseStartToFirstMeaningfulPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       FirstMeaningfulPaintNotRecorded) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));

  // Navigate away before a FMP is reported.
  NavigateToUntrackedUrl();

  histogram_tester_.ExpectUniqueSample(
      internal::kHistogramFirstMeaningfulPaintStatus,
      internal::FIRST_MEANINGFUL_PAINT_DID_NOT_REACH_NETWORK_STABLE, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramFirstMeaningfulPaint, 0);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramParseStartToFirstMeaningfulPaint, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       NoStatePrefetchObserverCacheable) {
  ASSERT_TRUE(embedded_test_server()->Start());

  scoped_refptr<TimingUpdatedObserver> fcp_observer =
      CreateTimingUpdatedObserver();
  fcp_observer->AddMatchingFields(
      TimingUpdatedObserver::FIRST_CONTENTFUL_PAINT);

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));

  fcp_observer->WaitForMatchingIPC();

  histogram_tester_.ExpectTotalCount(
      "Prerender.none_PrefetchTTFCP.Reference.NoStore.Visible", 0);
  histogram_tester_.ExpectTotalCount(
      "Prerender.none_PrefetchTTFCP.Reference.Cacheable.Visible", 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       NoStatePrefetchObserverNoStore) {
  ASSERT_TRUE(embedded_test_server()->Start());

  scoped_refptr<TimingUpdatedObserver> fcp_observer =
      CreateTimingUpdatedObserver();
  fcp_observer->AddMatchingFields(
      TimingUpdatedObserver::FIRST_CONTENTFUL_PAINT);

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/nostore.html"));

  fcp_observer->WaitForMatchingIPC();

  histogram_tester_.ExpectTotalCount(
      "Prerender.none_PrefetchTTFCP.Reference.NoStore.Visible", 1);
  histogram_tester_.ExpectTotalCount(
      "Prerender.none_PrefetchTTFCP.Reference.Cacheable.Visible", 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, CSSTiming) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Careful: Blink code clamps timestamps to 5us, so any CSS parsing we do here
  // must take >> 5us, otherwise we'll log 0 for the value and it will remain
  // unset here.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/page_load_metrics/page_with_css.html"));
  NavigateToUntrackedUrl();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                     1);
  histogram_tester_.ExpectTotalCount(
      "PageLoad.CSSTiming.Parse.BeforeFirstContentfulPaint", 1);
  histogram_tester_.ExpectTotalCount(
      "PageLoad.CSSTiming.Update.BeforeFirstContentfulPaint", 1);
  histogram_tester_.ExpectTotalCount(
      "PageLoad.CSSTiming.ParseAndUpdate.BeforeFirstContentfulPaint", 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PayloadSize) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(
                                              "/page_load_metrics/large.html"));
  NavigateToUntrackedUrl();

  histogram_tester_.ExpectTotalCount(internal::kHistogramTotalBytes, 1);

  // Verify that there is a single sample recorded in the 10kB bucket (the size
  // of the main HTML response).
  histogram_tester_.ExpectBucketCount(internal::kHistogramTotalBytes, 10, 1);
}
