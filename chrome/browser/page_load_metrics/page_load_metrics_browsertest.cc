// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/keep_alive_types.h"
#include "chrome/browser/lifetime/scoped_keep_alive.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/page_load_metrics/observers/aborts_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/core_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/document_write_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/no_state_prefetch_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_origin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/failing_http_transaction_factory.h"
#include "net/http/http_cache.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

void FailAllNetworkTransactions(net::URLRequestContextGetter* getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  net::HttpCache* cache(
      getter->GetURLRequestContext()->http_transaction_factory()->GetCache());
  DCHECK(cache);
  std::unique_ptr<net::FailingHttpTransactionFactory> factory =
      base::MakeUnique<net::FailingHttpTransactionFactory>(cache->GetSession(),
                                                           net::ERR_FAILED);
  // Throw away old version; since this is a browser test, there is no
  // need to restore the old state.
  cache->SetHttpNetworkTransactionFactoryForTesting(std::move(factory));
}

// Waits until specified timing and metadata expectations are satisfied.
class PageLoadMetricsWaiter
    : public page_load_metrics::MetricsWebContentsObserver::TestingObserver {
 public:
  // A bitvector to express which timing fields to match on.
  enum class TimingField : int {
    FIRST_LAYOUT = 1 << 0,
    FIRST_PAINT = 1 << 1,
    FIRST_CONTENTFUL_PAINT = 1 << 2,
    FIRST_MEANINGFUL_PAINT = 1 << 3,
    STYLE_UPDATE_BEFORE_FCP = 1 << 4,
    DOCUMENT_WRITE_BLOCK_RELOAD = 1 << 5,
    LOAD_EVENT = 1 << 6
  };

  explicit PageLoadMetricsWaiter(content::WebContents* web_contents)
      : TestingObserver(web_contents), weak_factory_(this) {}

  ~PageLoadMetricsWaiter() override {
    CHECK(did_add_observer_);
    CHECK_EQ(nullptr, run_loop_.get());
  }

  // Add a page-level expectation.
  void AddPageExpectation(TimingField field) {
    page_expected_fields_.Set(field);
  }

  // Add a subframe-level expectation.
  void AddSubFrameExpectation(TimingField field) {
    subframe_expected_fields_.Set(field);
    // If the given field is also a page-level field, then add a page-level
    // expectation as well
    if (IsPageLevelField(field))
      page_expected_fields_.Set(field);
  }

  // Whether the given TimingField was observed in the page.
  bool DidObserveInPage(TimingField field) {
    return observed_page_fields_.IsSet(field);
  }

  // Waits for a OnTimingUpdated call that matches the fields set by
  // |AddPageExpectation| and |AddSubFrameExpectation|. All matching fields
  // must be set in a OnTimingUpdated call for it to end this wait.
  void Wait() {
    if (expectations_satisfied())
      return;

    run_loop_ = base::MakeUnique<base::RunLoop>();
    run_loop_->Run();
    run_loop_ = nullptr;

    EXPECT_TRUE(expectations_satisfied());
  }

  void OnTimingUpdated(bool is_subframe,
                       const page_load_metrics::mojom::PageLoadTiming& timing,
                       const page_load_metrics::PageLoadExtraInfo& extra_info) {
    if (expectations_satisfied())
      return;

    const page_load_metrics::mojom::PageLoadMetadata& metadata =
        is_subframe ? extra_info.subframe_metadata
                    : extra_info.main_frame_metadata;
    TimingFieldBitSet matched_bits = GetMatchedBits(timing, metadata);
    if (is_subframe) {
      subframe_expected_fields_.ClearMatching(matched_bits);
    } else {
      page_expected_fields_.ClearMatching(matched_bits);
      observed_page_fields_.Merge(matched_bits);
    }

    if (expectations_satisfied() && run_loop_)
      run_loop_->Quit();
  }

 private:
  // PageLoadMetricsObserver used by the PageLoadMetricsWaiter to observe
  // metrics updates.
  class WaiterMetricsObserver
      : public page_load_metrics::PageLoadMetricsObserver {
   public:
    // We use a WeakPtr to the PageLoadMetricsWaiter because |waiter| can be
    // destroyed before this WaiterMetricsObserver.
    explicit WaiterMetricsObserver(base::WeakPtr<PageLoadMetricsWaiter> waiter)
        : waiter_(waiter) {}

    void OnTimingUpdate(
        bool is_subframe,
        const page_load_metrics::mojom::PageLoadTiming& timing,
        const page_load_metrics::PageLoadExtraInfo& extra_info) override {
      if (waiter_)
        waiter_->OnTimingUpdated(is_subframe, timing, extra_info);
    }

   private:
    const base::WeakPtr<PageLoadMetricsWaiter> waiter_;
  };

  // Manages a bitset of TimingFields.
  class TimingFieldBitSet {
   public:
    TimingFieldBitSet() {}

    // Returns whether this bitset has all bits unset.
    bool Empty() const { return bitmask_ == 0; }

    // Returns whether this bitset has the given bit set.
    bool IsSet(TimingField field) const {
      return (bitmask_ & static_cast<int>(field)) != 0;
    }

    // Sets the bit for the given |field|.
    void Set(TimingField field) { bitmask_ |= static_cast<int>(field); }

    // Merges bits set in |other| into this bitset.
    void Merge(const TimingFieldBitSet& other) { bitmask_ |= other.bitmask_; }

    // Clears all bits set in the |other| bitset.
    void ClearMatching(const TimingFieldBitSet& other) {
      bitmask_ &= ~other.bitmask_;
    }

   private:
    int bitmask_ = 0;
  };

  static bool IsPageLevelField(TimingField field) {
    switch (field) {
      case TimingField::FIRST_PAINT:
      case TimingField::FIRST_CONTENTFUL_PAINT:
      case TimingField::FIRST_MEANINGFUL_PAINT:
        return true;
      default:
        return false;
    }
  }

  static TimingFieldBitSet GetMatchedBits(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::mojom::PageLoadMetadata& metadata) {
    TimingFieldBitSet matched_bits;
    if (timing.document_timing->first_layout)
      matched_bits.Set(TimingField::FIRST_LAYOUT);
    if (timing.document_timing->load_event_start)
      matched_bits.Set(TimingField::LOAD_EVENT);
    if (timing.paint_timing->first_paint)
      matched_bits.Set(TimingField::FIRST_PAINT);
    if (timing.paint_timing->first_contentful_paint)
      matched_bits.Set(TimingField::FIRST_CONTENTFUL_PAINT);
    if (timing.paint_timing->first_meaningful_paint)
      matched_bits.Set(TimingField::FIRST_MEANINGFUL_PAINT);
    if (timing.style_sheet_timing->update_style_duration_before_fcp)
      matched_bits.Set(TimingField::STYLE_UPDATE_BEFORE_FCP);
    if (metadata.behavior_flags &
        blink::WebLoadingBehaviorFlag::
            kWebLoadingBehaviorDocumentWriteBlockReload)
      matched_bits.Set(TimingField::DOCUMENT_WRITE_BLOCK_RELOAD);

    return matched_bits;
  }

  void OnCommit(page_load_metrics::PageLoadTracker* tracker) override {
    // A PageLoadMetricsWaiter should only wait for events from a single page
    // load.
    ASSERT_FALSE(did_add_observer_);
    tracker->AddObserver(
        base::MakeUnique<WaiterMetricsObserver>(weak_factory_.GetWeakPtr()));
    did_add_observer_ = true;
  }

  bool expectations_satisfied() const {
    return subframe_expected_fields_.Empty() && page_expected_fields_.Empty();
  }

  std::unique_ptr<base::RunLoop> run_loop_;

  TimingFieldBitSet page_expected_fields_;
  TimingFieldBitSet subframe_expected_fields_;

  TimingFieldBitSet observed_page_fields_;

  bool did_add_observer_ = false;

  base::WeakPtrFactory<PageLoadMetricsWaiter> weak_factory_;
};

using TimingField = PageLoadMetricsWaiter::TimingField;

}  // namespace

class PageLoadMetricsBrowserTest : public InProcessBrowserTest {
 public:
  PageLoadMetricsBrowserTest() {}
  ~PageLoadMetricsBrowserTest() override {}

 protected:
  // Force navigation to a new page, so the currently tracked page load runs its
  // OnComplete callback. You should prefer to use PageLoadMetricsWaiter, and
  // only use NavigateToUntrackedUrl for cases where the waiter isn't
  // sufficient.
  void NavigateToUntrackedUrl() {
    ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  }

  bool NoPageLoadMetricsRecorded() {
    // Determine whether any 'public' page load metrics are recorded. We exclude
    // 'internal' metrics as these may be recorded for debugging purposes.
    size_t total_pageload_histograms =
        histogram_tester_.GetTotalCountsForPrefix("PageLoad.").size();
    size_t total_internal_histograms =
        histogram_tester_.GetTotalCountsForPrefix("PageLoad.Internal.").size();
    DCHECK_GE(total_pageload_histograms, total_internal_histograms);
    return total_pageload_histograms - total_internal_histograms == 0;
  }

  std::unique_ptr<PageLoadMetricsWaiter> CreatePageLoadMetricsWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return base::MakeUnique<PageLoadMetricsWaiter>(web_contents);
  }

  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageLoadMetricsBrowserTest);
};

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NoPageLoadMetricsRecorded());
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NewPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_PAINT);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstPaint, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramParseDuration, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramParseBlockedOnScriptLoad, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramParseBlockedOnScriptExecution, 1);

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  NavigateToUntrackedUrl();
  histogram_tester_.ExpectTotalCount(internal::kHistogramTotalBytes, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramPageTimingForegroundDuration, 1);

  // Verify that NoPageLoadMetricsRecorded returns false when PageLoad metrics
  // have been recorded.
  EXPECT_FALSE(NoPageLoadMetricsRecorded());
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NewPageInNewForegroundTab) {
  ASSERT_TRUE(embedded_test_server()->Start());

  chrome::NavigateParams params(browser(),
                                embedded_test_server()->GetURL("/title1.html"),
                                ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
  auto waiter = base::MakeUnique<PageLoadMetricsWaiter>(params.target_contents);
  waiter->AddPageExpectation(TimingField::LOAD_EVENT);
  waiter->Wait();

  // Due to crbug.com/725347, with browser side navigation enabled, navigations
  // in new tabs were recorded as starting in the background. Here we verify
  // that navigations initiated in a new tab are recorded as happening in the
  // foreground.
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kBackgroundHistogramLoad, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoPaintForEmptyDocument) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_LAYOUT);
  waiter->AddPageExpectation(TimingField::LOAD_EVENT);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/empty.html"));
  waiter->Wait();
  EXPECT_FALSE(waiter->DidObserveInPage(TimingField::FIRST_PAINT));

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstPaint, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                     0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       NoPaintForEmptyDocumentInChildFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL a_url(
      embedded_test_server()->GetURL("/page_load_metrics/empty_iframe.html"));

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_LAYOUT);
  waiter->AddPageExpectation(TimingField::LOAD_EVENT);
  waiter->AddSubFrameExpectation(TimingField::FIRST_LAYOUT);
  waiter->AddSubFrameExpectation(TimingField::LOAD_EVENT);
  ui_test_utils::NavigateToURL(browser(), a_url);
  waiter->Wait();
  EXPECT_FALSE(waiter->DidObserveInPage(TimingField::FIRST_PAINT));

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstPaint, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                     0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PaintInChildFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL a_url(embedded_test_server()->GetURL("/page_load_metrics/iframe.html"));
  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_LAYOUT);
  waiter->AddPageExpectation(TimingField::LOAD_EVENT);
  waiter->AddSubFrameExpectation(TimingField::FIRST_PAINT);
  waiter->AddSubFrameExpectation(TimingField::FIRST_CONTENTFUL_PAINT);
  ui_test_utils::NavigateToURL(browser(), a_url);
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PaintInMultipleChildFrames) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL a_url(embedded_test_server()->GetURL("/page_load_metrics/iframes.html"));

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_LAYOUT);
  waiter->AddPageExpectation(TimingField::LOAD_EVENT);
  waiter->AddSubFrameExpectation(TimingField::FIRST_PAINT);
  waiter->AddSubFrameExpectation(TimingField::FIRST_CONTENTFUL_PAINT);
  ui_test_utils::NavigateToURL(browser(), a_url);
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PaintInMainAndChildFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL a_url(embedded_test_server()->GetURL(
      "/page_load_metrics/main_frame_with_iframe.html"));

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_LAYOUT);
  waiter->AddPageExpectation(TimingField::LOAD_EVENT);
  waiter->AddPageExpectation(TimingField::FIRST_PAINT);
  waiter->AddPageExpectation(TimingField::FIRST_CONTENTFUL_PAINT);
  waiter->AddSubFrameExpectation(TimingField::FIRST_PAINT);
  waiter->AddSubFrameExpectation(TimingField::FIRST_CONTENTFUL_PAINT);
  ui_test_utils::NavigateToURL(browser(), a_url);
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, SameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_LAYOUT);
  waiter->AddPageExpectation(TimingField::LOAD_EVENT);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);

  // Perform a same-document navigation. No additional metrics should be logged.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html#hash"));
  NavigateToUntrackedUrl();

  histogram_tester_.ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, SameUrlNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_LAYOUT);
  waiter->AddPageExpectation(TimingField::LOAD_EVENT);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);

  waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_LAYOUT);
  waiter->AddPageExpectation(TimingField::LOAD_EVENT);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  waiter->Wait();

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
      base::BindOnce(&FailAllNetworkTransactions,
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

  base::ThreadRestrictions::ScopedAllowIO allow_io;
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

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_CONTENTFUL_PAINT);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_external_script.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteParseStartToFirstContentfulPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoPreloadDocumentWrite) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_CONTENTFUL_PAINT);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_no_script.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                     1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteParseStartToFirstContentfulPaint, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoDocumentWrite) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_CONTENTFUL_PAINT);

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                     1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteParseStartToFirstContentfulPaint, 0);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DocumentWriteBlock) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_CONTENTFUL_PAINT);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DocumentWriteReload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_CONTENTFUL_PAINT);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 1);

  // Reload should not log the histogram as the script is not blocked.
  waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::DOCUMENT_WRITE_BLOCK_RELOAD);
  waiter->AddPageExpectation(TimingField::FIRST_CONTENTFUL_PAINT);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockReloadCount, 1);

  waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::DOCUMENT_WRITE_BLOCK_RELOAD);
  waiter->AddPageExpectation(TimingField::FIRST_CONTENTFUL_PAINT);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockReloadCount, 2);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DocumentWriteAsync) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_CONTENTFUL_PAINT);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_async_script.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                     1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DocumentWriteSameDomain) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_CONTENTFUL_PAINT);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_external_script.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                     1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoDocumentWriteScript) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_CONTENTFUL_PAINT);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_no_script.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                     1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 0);
}

// TODO(crbug.com/712935): Flaky on Linux dbg.
#if defined(OS_LINUX) && !defined(NDEBUG)
#define MAYBE_BadXhtml DISABLED_BadXhtml
#else
#define MAYBE_BadXhtml BadXhtml
#endif
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, MAYBE_BadXhtml) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // When an XHTML page contains invalid XML, it causes a paint of the error
  // message without a layout. Page load metrics currently treats this as an
  // error. Eventually, we'll fix this by special casing the handling of
  // documents with non-well-formed XML on the blink side. See crbug.com/627607
  // for more.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/page_load_metrics/badxml.xhtml"));
  NavigateToUntrackedUrl();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstPaint, 0);

  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kErrorEvents,
      page_load_metrics::ERR_BAD_TIMING_IPC_INVALID_TIMING, 1);

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadTimingStatus, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTimingStatus,
      page_load_metrics::internal::INVALID_ORDER_FIRST_LAYOUT_FIRST_PAINT, 1);
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

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::LOAD_EVENT);
  chrome::Navigate(&params2);
  waiter->Wait();

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

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::LOAD_EVENT);
  chrome::Navigate(&params2);
  waiter->Wait();

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

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::LOAD_EVENT);
  chrome::Navigate(&params3);
  waiter->Wait();

  manager2.WaitForNavigationFinished();

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
    auto waiter = CreatePageLoadMetricsWaiter();
    waiter->AddPageExpectation(TimingField::LOAD_EVENT);
    EXPECT_TRUE(content::ExecuteScript(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.location.reload();"));
    waiter->Wait();
  }

  manager.WaitForNavigationFinished();

  EXPECT_TRUE(histogram_tester_
                  .GetTotalCountsForPrefix("PageLoad.Experimental.AbortTiming.")
                  .empty());
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       FirstMeaningfulPaintRecorded) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_MEANINGFUL_PAINT);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  waiter->Wait();

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

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_CONTENTFUL_PAINT);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/page_with_active_connections.html"));
  waiter->Wait();

  // Navigate away before a FMP is reported.
  NavigateToUntrackedUrl();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                     1);
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

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_CONTENTFUL_PAINT);

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));

  waiter->Wait();

  histogram_tester_.ExpectTotalCount(
      "Prerender.none_PrefetchTTFCP.Reference.NoStore.Visible", 0);
  histogram_tester_.ExpectTotalCount(
      "Prerender.none_PrefetchTTFCP.Reference.Cacheable.Visible", 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       NoStatePrefetchObserverNoStore) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::FIRST_CONTENTFUL_PAINT);

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/nostore.html"));

  waiter->Wait();

  histogram_tester_.ExpectTotalCount(
      "Prerender.none_PrefetchTTFCP.Reference.NoStore.Visible", 1);
  histogram_tester_.ExpectTotalCount(
      "Prerender.none_PrefetchTTFCP.Reference.Cacheable.Visible", 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, CSSTiming) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::STYLE_UPDATE_BEFORE_FCP);

  // Careful: Blink code clamps timestamps to 5us, so any CSS parsing we do here
  // must take >> 5us, otherwise we'll log 0 for the value and it will remain
  // unset here.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/page_load_metrics/page_with_css.html"));
  waiter->Wait();

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

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::LOAD_EVENT);
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(
                                              "/page_load_metrics/large.html"));
  waiter->Wait();

  // Payload histograms are only logged when a page load terminates, so force
  // navigation to another page.
  NavigateToUntrackedUrl();

  histogram_tester_.ExpectTotalCount(internal::kHistogramTotalBytes, 1);

  // Verify that there is a single sample recorded in the 10kB bucket (the size
  // of the main HTML response).
  histogram_tester_.ExpectBucketCount(internal::kHistogramTotalBytes, 10, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PayloadSizeChildFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsWaiter();
  waiter->AddPageExpectation(TimingField::LOAD_EVENT);
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/page_load_metrics/large_iframe.html"));
  waiter->Wait();

  // Payload histograms are only logged when a page load terminates, so force
  // navigation to another page.
  NavigateToUntrackedUrl();

  histogram_tester_.ExpectTotalCount(internal::kHistogramTotalBytes, 1);

  // Verify that there is a single sample recorded in the 10kB bucket (the size
  // of the iframe response).
  histogram_tester_.ExpectBucketCount(internal::kHistogramTotalBytes, 10, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       PayloadSizeIgnoresDownloads) {
  ASSERT_TRUE(embedded_test_server()->Start());

  bool prev_io_allowed = base::ThreadRestrictions::SetIOAllowed(true);
  {
    base::ScopedTempDir downloads_directory;
    ASSERT_TRUE(downloads_directory.CreateUniqueTempDir());
    browser()->profile()->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory, downloads_directory.GetPath());
    content::DownloadTestObserverTerminal downloads_observer(
        content::BrowserContext::GetDownloadManager(browser()->profile()),
        1,  // == wait_count (only waiting for "download-test1.lib").
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(
                       "/page_load_metrics/download_anchor_click.html"));
    downloads_observer.WaitForFinished();
  }
  base::ThreadRestrictions::SetIOAllowed(prev_io_allowed);

  NavigateToUntrackedUrl();

  histogram_tester_.ExpectUniqueSample(internal::kHistogramTotalBytes, 0, 1);
}

class SessionRestorePageLoadMetricsBrowserTest
    : public PageLoadMetricsBrowserTest {
 public:
  SessionRestorePageLoadMetricsBrowserTest() {}

  // PageLoadMetricsBrowserTest:
  void SetUpOnMainThread() override {
    PageLoadMetricsBrowserTest::SetUpOnMainThread();
    SessionStartupPref::SetStartupPref(
        browser()->profile(), SessionStartupPref(SessionStartupPref::LAST));
    ASSERT_TRUE(embedded_test_server()->Start());
#if defined(OS_CHROMEOS)
    SessionServiceTestHelper helper(
        SessionServiceFactory::GetForProfile(browser()->profile()));
    helper.SetForceBrowserNotAliveWithNoWindows(true);
    helper.ReleaseService();
#endif
  }

  Browser* QuitBrowserAndRestore(Browser* browser) {
    Profile* profile = browser->profile();

    std::unique_ptr<ScopedKeepAlive> keep_alive(new ScopedKeepAlive(
        KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED));
    CloseBrowserSynchronously(browser);

    // Create a new window, which should trigger session restore.
    chrome::NewEmptyWindow(profile);
    ui_test_utils::BrowserAddedObserver window_observer;
    return window_observer.WaitForSingleNewBrowser();
  }

  void WaitForTabsToLoad(Browser* browser) {
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
      content::WebContents* contents =
          browser->tab_strip_model()->GetWebContentsAt(i);
      contents->GetController().LoadIfNecessary();
      content::WaitForLoadStop(contents);
    }
  }

  GURL GetTestURL() const {
    return embedded_test_server()->GetURL("/title1.html");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionRestorePageLoadMetricsBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SessionRestorePageLoadMetricsBrowserTest,
                       InitialVisibilityOfSingleRestoredTab) {
  ui_test_utils::NavigateToURL(browser(), GetTestURL());
  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, true, 1);

  Browser* new_browser = QuitBrowserAndRestore(browser());
  WaitForTabsToLoad(new_browser);

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, 2);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, true, 2);
}

IN_PROC_BROWSER_TEST_F(SessionRestorePageLoadMetricsBrowserTest,
                       InitialVisibilityOfMultipleRestoredTabs) {
  ui_test_utils::NavigateToURL(browser(), GetTestURL());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetTestURL(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, 2);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, false, 1);

  Browser* new_browser = QuitBrowserAndRestore(browser());
  WaitForTabsToLoad(new_browser);

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_TRUE(tab_strip);
  ASSERT_EQ(2, tab_strip->count());

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, 4);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, true, 2);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, false, 2);
}
