// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#if defined(OS_ANDROID)
#include "chrome/browser/page_load_metrics/observers/android_page_load_metrics_observer.h"
#endif  // OS_ANDROID
#include "chrome/browser/page_load_metrics/observers/aborts_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/amp_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/core_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/css_scanning_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/delay_navigation_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/document_write_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/from_gws_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/google_captcha_observer.h"
#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/media_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/no_state_prefetch_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/omnibox_suggestion_used_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/prerender_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/previews_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/protocol_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/resource_prefetch_predictor_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/service_worker_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/subresource_filter_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/tab_restore_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/ukm_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_embedder_interface.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "components/rappor/rappor_service_impl.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace chrome {

namespace {

class PageLoadMetricsEmbedder
    : public page_load_metrics::PageLoadMetricsEmbedderInterface {
 public:
  explicit PageLoadMetricsEmbedder(content::WebContents* web_contents);
  ~PageLoadMetricsEmbedder() override;

  // page_load_metrics::PageLoadMetricsEmbedderInterface:
  bool IsNewTabPageUrl(const GURL& url) override;
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override;

 private:
  bool IsPrerendering() const;

  content::WebContents* const web_contents_;

  DISALLOW_COPY_AND_ASSIGN(PageLoadMetricsEmbedder);
};

PageLoadMetricsEmbedder::PageLoadMetricsEmbedder(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

PageLoadMetricsEmbedder::~PageLoadMetricsEmbedder() {}

void PageLoadMetricsEmbedder::RegisterObservers(
    page_load_metrics::PageLoadTracker* tracker) {
  if (!IsPrerendering()) {
    tracker->AddObserver(base::MakeUnique<AbortsPageLoadMetricsObserver>());
    tracker->AddObserver(base::MakeUnique<AMPPageLoadMetricsObserver>());
    tracker->AddObserver(base::MakeUnique<CorePageLoadMetricsObserver>());
    tracker->AddObserver(
        base::MakeUnique<
            data_reduction_proxy::DataReductionProxyMetricsObserver>());
    tracker->AddObserver(base::MakeUnique<FromGWSPageLoadMetricsObserver>());
    tracker->AddObserver(
        base::MakeUnique<google_captcha_observer::GoogleCaptchaObserver>());
    tracker->AddObserver(
        base::MakeUnique<DocumentWritePageLoadMetricsObserver>());
    tracker->AddObserver(base::MakeUnique<MediaPageLoadMetricsObserver>());
    tracker->AddObserver(
        base::MakeUnique<previews::PreviewsPageLoadMetricsObserver>());
    tracker->AddObserver(
        base::MakeUnique<ServiceWorkerPageLoadMetricsObserver>());
    tracker->AddObserver(base::MakeUnique<SubresourceFilterMetricsObserver>());
    tracker->AddObserver(
        base::MakeUnique<HttpsEngagementPageLoadMetricsObserver>(
            web_contents_->GetBrowserContext()));
    tracker->AddObserver(base::MakeUnique<CssScanningMetricsObserver>());
    tracker->AddObserver(base::MakeUnique<ProtocolPageLoadMetricsObserver>());
    tracker->AddObserver(base::MakeUnique<TabRestorePageLoadMetricsObserver>());

    std::unique_ptr<page_load_metrics::PageLoadMetricsObserver> ukm_observer =
        UkmPageLoadMetricsObserver::CreateIfNeeded(web_contents_);
    if (ukm_observer)
      tracker->AddObserver(std::move(ukm_observer));

    std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
        no_state_prefetch_observer =
            NoStatePrefetchPageLoadMetricsObserver::CreateIfNeeded(
                web_contents_);
    if (no_state_prefetch_observer)
      tracker->AddObserver(std::move(no_state_prefetch_observer));
#if defined(OS_ANDROID)
    tracker->AddObserver(
        base::MakeUnique<AndroidPageLoadMetricsObserver>(web_contents_));
#endif  // OS_ANDROID
    std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
        resource_prefetch_predictor_observer =
            ResourcePrefetchPredictorPageLoadMetricsObserver::CreateIfNeeded(
                web_contents_);
    if (resource_prefetch_predictor_observer)
      tracker->AddObserver(std::move(resource_prefetch_predictor_observer));
  } else {
    std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
        prerender_observer =
            PrerenderPageLoadMetricsObserver::CreateIfNeeded(web_contents_);
    if (prerender_observer)
      tracker->AddObserver(std::move(prerender_observer));
  }
  tracker->AddObserver(
      base::MakeUnique<OmniboxSuggestionUsedMetricsObserver>(IsPrerendering()));
  tracker->AddObserver(
      base::MakeUnique<DelayNavigationPageLoadMetricsObserver>());
}

bool PageLoadMetricsEmbedder::IsPrerendering() const {
  return prerender::PrerenderContents::FromWebContents(web_contents_) !=
         nullptr;
}

bool PageLoadMetricsEmbedder::IsNewTabPageUrl(const GURL& url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (!profile)
    return false;
  return search::IsInstantNTPURL(url, profile);
}

}  // namespace

void InitializePageLoadMetricsForWebContents(
    content::WebContents* web_contents) {
  page_load_metrics::MetricsWebContentsObserver::CreateForWebContents(
      web_contents, base::MakeUnique<PageLoadMetricsEmbedder>(web_contents));
}

}  // namespace chrome
