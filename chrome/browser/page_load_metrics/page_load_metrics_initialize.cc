// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/page_load_metrics/observers/aborts_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/core_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/document_write_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/from_gws_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/google_captcha_observer.h"
#include "chrome/browser/page_load_metrics/observers/stale_while_revalidate_metrics_observer.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/rappor/rappor_service.h"
#include "content/public/browser/web_contents.h"

namespace chrome {

void InitializePageLoadMetricsForWebContents(
    content::WebContents* web_contents) {
  page_load_metrics::MetricsWebContentsObserver::CreateForWebContents(
      web_contents, base::WrapUnique(new PageLoadMetricsEmbedder()));
}

PageLoadMetricsEmbedder::~PageLoadMetricsEmbedder() {}

void PageLoadMetricsEmbedder::RegisterObservers(
    page_load_metrics::PageLoadTracker* tracker) {
  // These classes are owned by the metrics.
  tracker->AddObserver(base::WrapUnique(new AbortsPageLoadMetricsObserver()));
  tracker->AddObserver(base::WrapUnique(new CorePageLoadMetricsObserver()));
  tracker->AddObserver(base::WrapUnique(new FromGWSPageLoadMetricsObserver()));
  tracker->AddObserver(
      base::WrapUnique(new google_captcha_observer::GoogleCaptchaObserver()));
  // TODO(ricea): Remove this in April 2016 or before. crbug.com/348877
  tracker->AddObserver(
      base::WrapUnique(new chrome::StaleWhileRevalidateMetricsObserver()));
  tracker->AddObserver(
      base::WrapUnique(new DocumentWritePageLoadMetricsObserver()));
}

bool PageLoadMetricsEmbedder::IsPrerendering(
    content::WebContents* web_contents) {
  return prerender::PrerenderContents::FromWebContents(web_contents) != nullptr;
}

}  // namespace chrome
