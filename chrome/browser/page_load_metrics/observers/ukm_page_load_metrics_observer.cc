// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ukm_page_load_metrics_observer.h"
#include "chrome/browser/browser_process.h"
#include "components/ukm/ukm_service.h"
#include "components/ukm/ukm_source.h"

// static
std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
UkmPageLoadMetricsObserver::CreateIfNeeded() {
  if (!g_browser_process->ukm_service()) {
    return nullptr;
  }

  return base::MakeUnique<UkmPageLoadMetricsObserver>();
}

UkmPageLoadMetricsObserver::UkmPageLoadMetricsObserver() {}

UkmPageLoadMetricsObserver::ObservePolicy UkmPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return started_in_foreground ? CONTINUE_OBSERVING : STOP_OBSERVING;
}

UkmPageLoadMetricsObserver::ObservePolicy
UkmPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  SendMetricsToUkm(timing, info);
  return STOP_OBSERVING;
}

UkmPageLoadMetricsObserver::ObservePolicy UkmPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  SendMetricsToUkm(timing, info);
  return STOP_OBSERVING;
}

void UkmPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  SendMetricsToUkm(timing, info);
}

void UkmPageLoadMetricsObserver::SendMetricsToUkm(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!info.did_commit || !timing.first_contentful_paint)
    return;

  ukm::UkmService* ukm_service = g_browser_process->ukm_service();
  DCHECK(ukm_service);

  std::unique_ptr<ukm::UkmSource> source = base::MakeUnique<ukm::UkmSource>();
  source->set_committed_url(info.url);
  source->set_first_contentful_paint(timing.first_contentful_paint.value());

  ukm_service->RecordSource(std::move(source));
}
