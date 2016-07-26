// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/page_load_metrics/observers/aborts_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/core_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/document_write_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/from_gws_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/google_captcha_observer.h"
#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/service_worker_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/stale_while_revalidate_metrics_observer.h"
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
  tracker->AddObserver(base::WrapUnique(new AbortsPageLoadMetricsObserver()));
  tracker->AddObserver(base::WrapUnique(new CorePageLoadMetricsObserver()));
  tracker->AddObserver(base::WrapUnique(
      new data_reduction_proxy::DataReductionProxyMetricsObserver()));
  tracker->AddObserver(base::WrapUnique(new FromGWSPageLoadMetricsObserver()));
  tracker->AddObserver(
      base::WrapUnique(new google_captcha_observer::GoogleCaptchaObserver()));
  // TODO(ricea): Remove this in April 2016 or before. crbug.com/348877
  tracker->AddObserver(
      base::WrapUnique(new chrome::StaleWhileRevalidateMetricsObserver()));
  tracker->AddObserver(
      base::WrapUnique(new DocumentWritePageLoadMetricsObserver()));
  tracker->AddObserver(
      base::WrapUnique(new ServiceWorkerPageLoadMetricsObserver()));
  tracker->AddObserver(
      base::WrapUnique(new HttpsEngagementPageLoadMetricsObserver(
          web_contents_->GetBrowserContext())));
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
      web_contents,
      base::WrapUnique(new PageLoadMetricsEmbedder(web_contents)));
}

}  // namespace chrome
