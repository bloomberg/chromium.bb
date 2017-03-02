// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ukm_page_load_metrics_observer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "components/ukm/ukm_entry_builder.h"
#include "components/ukm/ukm_service.h"

namespace internal {

const char kUkmPageLoadEventName[] = "PageLoad";
const char kUkmParseStartName[] = "ParseTiming.ParseStart";
const char kUkmDomContentLoadedName[] =
    "DocumentTiming.NavigationToDOMContentLoadedEventFired";
const char kUkmLoadEventName[] = "DocumentTiming.NavigationToLoadEventFired";
const char kUkmFirstContentfulPaintName[] =
    "PaintTiming.NavigationToFirstContentfulPaint";
const char kUkmFirstMeaningfulPaintName[] =
    "Experimental.PaintTiming.NavigationToFirstMeaningfulPaint";
const char kUkmForegroundDurationName[] = "PageTiming.ForegroundDuration";

}  // namespace internal

// static
std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
UkmPageLoadMetricsObserver::CreateIfNeeded() {
  if (!g_browser_process->ukm_service()) {
    return nullptr;
  }

  return base::MakeUnique<UkmPageLoadMetricsObserver>();
}

UkmPageLoadMetricsObserver::UkmPageLoadMetricsObserver()
    : source_id_(ukm::UkmService::GetNewSourceID()) {}

UkmPageLoadMetricsObserver::~UkmPageLoadMetricsObserver() = default;

UkmPageLoadMetricsObserver::ObservePolicy UkmPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  if (!started_in_foreground)
    return STOP_OBSERVING;

  ukm::UkmService* ukm_service = g_browser_process->ukm_service();
  ukm_service->UpdateSourceURL(source_id_, navigation_handle->GetURL());
  return CONTINUE_OBSERVING;
}

UkmPageLoadMetricsObserver::ObservePolicy
UkmPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordTimingMetrics(timing);
  RecordPageLoadExtraInfoMetrics(info, base::TimeTicks::Now());
  return STOP_OBSERVING;
}

UkmPageLoadMetricsObserver::ObservePolicy UkmPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordTimingMetrics(timing);
  RecordPageLoadExtraInfoMetrics(
      info, base::TimeTicks() /* no app_background_time */);
  return STOP_OBSERVING;
}

void UkmPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  RecordPageLoadExtraInfoMetrics(
      extra_info, base::TimeTicks() /* no app_background_time */);
}

void UkmPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordTimingMetrics(timing);
  RecordPageLoadExtraInfoMetrics(
      info, base::TimeTicks() /* no app_background_time */);
}

void UkmPageLoadMetricsObserver::RecordTimingMetrics(
    const page_load_metrics::PageLoadTiming& timing) {
  ukm::UkmService* ukm_service = g_browser_process->ukm_service();
  std::unique_ptr<ukm::UkmEntryBuilder> builder =
      ukm_service->GetEntryBuilder(source_id_, internal::kUkmPageLoadEventName);
  if (timing.parse_start) {
    builder->AddMetric(internal::kUkmParseStartName,
                       timing.parse_start.value().InMilliseconds());
  }
  if (timing.dom_content_loaded_event_start) {
    builder->AddMetric(
        internal::kUkmDomContentLoadedName,
        timing.dom_content_loaded_event_start.value().InMilliseconds());
  }
  if (timing.load_event_start) {
    builder->AddMetric(internal::kUkmLoadEventName,
                       timing.load_event_start.value().InMilliseconds());
  }
  if (timing.first_contentful_paint) {
    builder->AddMetric(internal::kUkmFirstContentfulPaintName,
                       timing.first_contentful_paint.value().InMilliseconds());
  }
  if (timing.first_meaningful_paint) {
    builder->AddMetric(internal::kUkmFirstMeaningfulPaintName,
                       timing.first_meaningful_paint.value().InMilliseconds());
  }
}

void UkmPageLoadMetricsObserver::RecordPageLoadExtraInfoMetrics(
    const page_load_metrics::PageLoadExtraInfo& info,
    base::TimeTicks app_background_time) {
  ukm::UkmService* ukm_service = g_browser_process->ukm_service();
  ukm_service->UpdateSourceURL(source_id_, info.url);

  base::Optional<base::TimeDelta> foreground_duration =
      page_load_metrics::GetInitialForegroundDuration(info,
                                                      app_background_time);
  if (foreground_duration) {
    std::unique_ptr<ukm::UkmEntryBuilder> builder =
        ukm_service->GetEntryBuilder(source_id_,
                                     internal::kUkmPageLoadEventName);
    builder->AddMetric(internal::kUkmForegroundDurationName,
                       foreground_duration.value().InMilliseconds());
  }
}
