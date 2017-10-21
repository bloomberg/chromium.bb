// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ukm_page_load_metrics_observer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service_factory.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace internal {

const char kUkmPageLoadEventName[] = "PageLoad";
const char kUkmParseStartName[] = "ParseTiming.NavigationToParseStart";
const char kUkmDomContentLoadedName[] =
    "DocumentTiming.NavigationToDOMContentLoadedEventFired";
const char kUkmLoadEventName[] = "DocumentTiming.NavigationToLoadEventFired";
const char kUkmFirstPaintName[] = "PaintTiming.NavigationToFirstPaint";
const char kUkmFirstContentfulPaintName[] =
    "PaintTiming.NavigationToFirstContentfulPaint";
const char kUkmFirstMeaningfulPaintName[] =
    "Experimental.PaintTiming.NavigationToFirstMeaningfulPaint";
const char kUkmForegroundDurationName[] = "PageTiming.ForegroundDuration";
const char kUkmFailedProvisionaLoadName[] =
    "PageTiming.NavigationToFailedProvisionalLoad";
const char kUkmEffectiveConnectionType[] =
    "Net.EffectiveConnectionType2.OnNavigationStart";
const char kUkmHttpRttEstimate[] = "Net.HttpRttEstimate.OnNavigationStart";
const char kUkmTransportRttEstimate[] =
    "Net.TransportRttEstimate.OnNavigationStart";
const char kUkmDownstreamKbpsEstimate[] =
    "Net.DownstreamKbpsEstimate.OnNavigationStart";
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
  if (!ukm::UkmRecorder::Get()) {
    return nullptr;
  }
  return base::MakeUnique<UkmPageLoadMetricsObserver>(
      GetNQEService(web_contents));
}

UkmPageLoadMetricsObserver::UkmPageLoadMetricsObserver(
    net::NetworkQualityEstimator::NetworkQualityProvider*
        network_quality_provider)
    : network_quality_provider_(network_quality_provider) {}

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
    http_rtt_estimate_ = network_quality_provider_->GetHttpRTT();
    transport_rtt_estimate_ = network_quality_provider_->GetTransportRTT();
    downstream_kbps_estimate_ =
        network_quality_provider_->GetDownstreamThroughputKbps();
  }
  page_transition_ = navigation_handle->GetPageTransition();
  return CONTINUE_OBSERVING;
}

UkmPageLoadMetricsObserver::ObservePolicy UkmPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  // The PageTransition for the navigation may be updated on commit.
  page_transition_ = navigation_handle->GetPageTransition();
  return CONTINUE_OBSERVING;
}

UkmPageLoadMetricsObserver::ObservePolicy
UkmPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordPageLoadExtraInfoMetrics(info, base::TimeTicks::Now());
  RecordTimingMetrics(timing, info.source_id);
  return STOP_OBSERVING;
}

UkmPageLoadMetricsObserver::ObservePolicy UkmPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordPageLoadExtraInfoMetrics(
      info, base::TimeTicks() /* no app_background_time */);
  RecordTimingMetrics(timing, info.source_id);
  return STOP_OBSERVING;
}

void UkmPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  RecordPageLoadExtraInfoMetrics(
      extra_info, base::TimeTicks() /* no app_background_time */);

  // Error codes have negative values, however we log net error code enum values
  // for UMA histograms using the equivalent positive value. For consistency in
  // UKM, we convert to a positive value here.
  int64_t net_error_code = static_cast<int64_t>(failed_load_info.error) * -1;
  DCHECK_GE(net_error_code, 0);
  ukm::builders::PageLoad(extra_info.source_id)
      .SetNet_ErrorCode_OnFailedProvisionalLoad(net_error_code)
      .SetPageTiming_NavigationToFailedProvisionalLoad(
          failed_load_info.time_to_failed_provisional_load.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordPageLoadExtraInfoMetrics(
      info, base::TimeTicks() /* no app_background_time */);
  RecordTimingMetrics(timing, info.source_id);
}

void UkmPageLoadMetricsObserver::RecordTimingMetrics(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    ukm::SourceId source_id) {
  ukm::builders::PageLoad builder(source_id);
  if (timing.parse_timing->parse_start) {
    builder.SetParseTiming_NavigationToParseStart(
        timing.parse_timing->parse_start.value().InMilliseconds());
  }
  if (timing.document_timing->dom_content_loaded_event_start) {
    builder.SetDocumentTiming_NavigationToDOMContentLoadedEventFired(
        timing.document_timing->dom_content_loaded_event_start.value()
            .InMilliseconds());
  }
  if (timing.document_timing->load_event_start) {
    builder.SetDocumentTiming_NavigationToLoadEventFired(
        timing.document_timing->load_event_start.value().InMilliseconds());
  }
  if (timing.paint_timing->first_paint) {
    builder.SetPaintTiming_NavigationToFirstPaint(
        timing.paint_timing->first_paint.value().InMilliseconds());
  }
  if (timing.paint_timing->first_contentful_paint) {
    builder.SetPaintTiming_NavigationToFirstContentfulPaint(
        timing.paint_timing->first_contentful_paint.value().InMilliseconds());
  }
  if (timing.paint_timing->first_meaningful_paint) {
    builder.SetExperimental_PaintTiming_NavigationToFirstMeaningfulPaint(
        timing.paint_timing->first_meaningful_paint.value().InMilliseconds());
  }
  builder.Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::RecordPageLoadExtraInfoMetrics(
    const page_load_metrics::PageLoadExtraInfo& info,
    base::TimeTicks app_background_time) {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  std::unique_ptr<ukm::UkmEntryBuilder> builder = ukm_recorder->GetEntryBuilder(
      info.source_id, internal::kUkmPageLoadEventName);
  base::Optional<base::TimeDelta> foreground_duration =
      page_load_metrics::GetInitialForegroundDuration(info,
                                                      app_background_time);
  if (foreground_duration) {
    builder->AddMetric(internal::kUkmForegroundDurationName,
                       foreground_duration.value().InMilliseconds());
  }

  // Convert to the EffectiveConnectionType as used in SystemProfileProto
  // before persisting the metric.
  metrics::SystemProfileProto::Network::EffectiveConnectionType
      proto_effective_connection_type =
          metrics::ConvertEffectiveConnectionType(effective_connection_type_);
  if (proto_effective_connection_type !=
      metrics::SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    builder->AddMetric(internal::kUkmEffectiveConnectionType,
                       static_cast<int64_t>(proto_effective_connection_type));
  }

  if (http_rtt_estimate_) {
    builder->AddMetric(
        internal::kUkmHttpRttEstimate,
        static_cast<int64_t>(http_rtt_estimate_.value().InMilliseconds()));
  }
  if (transport_rtt_estimate_) {
    builder->AddMetric(
        internal::kUkmTransportRttEstimate,
        static_cast<int64_t>(transport_rtt_estimate_.value().InMilliseconds()));
  }
  if (downstream_kbps_estimate_) {
    builder->AddMetric(internal::kUkmDownstreamKbpsEstimate,
                       static_cast<int64_t>(downstream_kbps_estimate_.value()));
  }
  // page_transition_ fits in a uint32_t, so we can safely cast to int64_t.
  builder->AddMetric(internal::kUkmPageTransition,
                     static_cast<int64_t>(page_transition_));
}
