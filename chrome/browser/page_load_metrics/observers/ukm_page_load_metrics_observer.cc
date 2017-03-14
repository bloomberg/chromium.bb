// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ukm_page_load_metrics_observer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service_factory.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/ukm/ukm_entry_builder.h"
#include "components/ukm/ukm_service.h"
#include "content/public/browser/web_contents.h"

namespace internal {

const char kUkmPageLoadEventName[] = "PageLoad";
const char kUkmParseStartName[] = "ParseTiming.NavigationToParseStart";
const char kUkmDomContentLoadedName[] =
    "DocumentTiming.NavigationToDOMContentLoadedEventFired";
const char kUkmLoadEventName[] = "DocumentTiming.NavigationToLoadEventFired";
const char kUkmFirstContentfulPaintName[] =
    "PaintTiming.NavigationToFirstContentfulPaint";
const char kUkmFirstMeaningfulPaintName[] =
    "Experimental.PaintTiming.NavigationToFirstMeaningfulPaint";
const char kUkmForegroundDurationName[] = "PageTiming.ForegroundDuration";
const char kUkmFailedProvisionaLoadName[] =
    "PageTiming.NavigationToFailedProvisionalLoad";
const char kUkmEffectiveConnectionType[] =
    "Net.EffectiveConnectionType.OnNavigationStart";
const char kUkmNetErrorCode[] = "Net.ErrorCode.OnFailedProvisionalLoad";
const char kUkmPageTransition[] = "Navigation.PageTransition";

}  // namespace internal

namespace {

UINetworkQualityEstimatorService* GetNQEService(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return nullptr;
  return UINetworkQualityEstimatorServiceFactory::GetForProfile(profile);
}

}  // namespace

// static
std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
UkmPageLoadMetricsObserver::CreateIfNeeded(content::WebContents* web_contents) {
  if (!g_browser_process->ukm_service()) {
    return nullptr;
  }
  return base::MakeUnique<UkmPageLoadMetricsObserver>(
      GetNQEService(web_contents));
}

UkmPageLoadMetricsObserver::UkmPageLoadMetricsObserver(
    net::NetworkQualityEstimator::NetworkQualityProvider*
        network_quality_provider)
    : network_quality_provider_(network_quality_provider),
      source_id_(ukm::UkmService::GetNewSourceID()) {}

UkmPageLoadMetricsObserver::~UkmPageLoadMetricsObserver() = default;

UkmPageLoadMetricsObserver::ObservePolicy UkmPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  if (!started_in_foreground)
    return STOP_OBSERVING;

  // When OnStart is invoked, we don't yet know whether we're observing a web
  // page load, vs another kind of load (e.g. a download or a PDF). Thus,
  // metrics and source information should not be recorded here. Instead, we
  // store data we might want to persist in member variables below, and later
  // record UKM metrics for that data once we've confirmed that we're observing
  // a web page load.

  if (network_quality_provider_) {
    effective_connection_type_ =
        network_quality_provider_->GetEffectiveConnectionType();
  }
  page_transition_ = navigation_handle->GetPageTransition();
  return CONTINUE_OBSERVING;
}

UkmPageLoadMetricsObserver::ObservePolicy UkmPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  // The PageTransition for the navigation may be updated on commit.
  page_transition_ = navigation_handle->GetPageTransition();
  return CONTINUE_OBSERVING;
}

UkmPageLoadMetricsObserver::ObservePolicy
UkmPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordPageLoadExtraInfoMetrics(info, base::TimeTicks::Now());
  RecordTimingMetrics(timing);
  return STOP_OBSERVING;
}

UkmPageLoadMetricsObserver::ObservePolicy UkmPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordPageLoadExtraInfoMetrics(
      info, base::TimeTicks() /* no app_background_time */);
  RecordTimingMetrics(timing);
  return STOP_OBSERVING;
}

void UkmPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  RecordPageLoadExtraInfoMetrics(
      extra_info, base::TimeTicks() /* no app_background_time */);

  ukm::UkmService* ukm_service = g_browser_process->ukm_service();
  std::unique_ptr<ukm::UkmEntryBuilder> builder =
      ukm_service->GetEntryBuilder(source_id_, internal::kUkmPageLoadEventName);
  // Error codes have negative values, however we log net error code enum values
  // for UMA histograms using the equivalent positive value. For consistency in
  // UKM, we convert to a positive value here.
  int64_t net_error_code = static_cast<int64_t>(failed_load_info.error) * -1;
  DCHECK_GE(net_error_code, 0);
  builder->AddMetric(internal::kUkmNetErrorCode, net_error_code);
  builder->AddMetric(
      internal::kUkmFailedProvisionaLoadName,
      failed_load_info.time_to_failed_provisional_load.InMilliseconds());
}

void UkmPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordPageLoadExtraInfoMetrics(
      info, base::TimeTicks() /* no app_background_time */);
  RecordTimingMetrics(timing);
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
  ukm_service->UpdateSourceURL(source_id_, info.start_url);
  ukm_service->UpdateSourceURL(source_id_, info.url);

  std::unique_ptr<ukm::UkmEntryBuilder> builder =
      ukm_service->GetEntryBuilder(source_id_, internal::kUkmPageLoadEventName);
  base::Optional<base::TimeDelta> foreground_duration =
      page_load_metrics::GetInitialForegroundDuration(info,
                                                      app_background_time);
  if (foreground_duration) {
    builder->AddMetric(internal::kUkmForegroundDurationName,
                       foreground_duration.value().InMilliseconds());
  }
  if (effective_connection_type_ != net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    builder->AddMetric(internal::kUkmEffectiveConnectionType,
                       static_cast<int64_t>(effective_connection_type_));
  }
  // page_transition_ fits in a uint32_t, so we can safely cast to int64_t.
  builder->AddMetric(internal::kUkmPageTransition,
                     static_cast<int64_t>(page_transition_));
}
