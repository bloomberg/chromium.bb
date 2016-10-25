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
#include "chrome/browser/page_load_metrics/observers/core_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/css_scanning_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/document_write_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/from_gws_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/google_captcha_observer.h"
#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/no_state_prefetch_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/previews_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/service_worker_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_embedder_interface.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "components/rappor/rappor_service.h"
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
  bool IsPrerendering(content::WebContents* web_contents) override;
  bool IsNewTabPageUrl(const GURL& url) override;
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override;

 private:
  content::WebContents* const web_contents_;

  DISALLOW_COPY_AND_ASSIGN(PageLoadMetricsEmbedder);
};

PageLoadMetricsEmbedder::PageLoadMetricsEmbedder(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

PageLoadMetricsEmbedder::~PageLoadMetricsEmbedder() {}

void PageLoadMetricsEmbedder::RegisterObservers(
    page_load_metrics::PageLoadTracker* tracker) {
  // These classes are owned by the metrics.
  tracker->AddObserver(base::MakeUnique<AbortsPageLoadMetricsObserver>());
  tracker->AddObserver(base::MakeUnique<CorePageLoadMetricsObserver>());
  tracker->AddObserver(
      base::MakeUnique<
          data_reduction_proxy::DataReductionProxyMetricsObserver>());
  tracker->AddObserver(base::MakeUnique<FromGWSPageLoadMetricsObserver>());
  tracker->AddObserver(
      base::MakeUnique<google_captcha_observer::GoogleCaptchaObserver>());
  tracker->AddObserver(
      base::MakeUnique<DocumentWritePageLoadMetricsObserver>());
  tracker->AddObserver(
      base::WrapUnique(new previews::PreviewsPageLoadMetricsObserver()));
  tracker->AddObserver(
      base::MakeUnique<ServiceWorkerPageLoadMetricsObserver>());
  tracker->AddObserver(base::MakeUnique<HttpsEngagementPageLoadMetricsObserver>(
      web_contents_->GetBrowserContext()));
  tracker->AddObserver(base::WrapUnique(new CssScanningMetricsObserver()));
  std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
      no_state_prefetch_observer =
          NoStatePrefetchPageLoadMetricsObserver::CreateIfNeeded(web_contents_);
  if (no_state_prefetch_observer)
    tracker->AddObserver(std::move(no_state_prefetch_observer));
#if defined(OS_ANDROID)
  tracker->AddObserver(
      base::MakeUnique<AndroidPageLoadMetricsObserver>(web_contents_));
#endif  // OS_ANDROID
}

bool PageLoadMetricsEmbedder::IsPrerendering(
    content::WebContents* web_contents) {
  return prerender::PrerenderContents::FromWebContents(web_contents) != nullptr;
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
