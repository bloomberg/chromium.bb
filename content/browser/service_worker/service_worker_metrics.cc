// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_util.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

namespace {

void RecordURLMetricOnUI(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetContentClient()->browser()->RecordURLMetric(
      "ServiceWorker.ControlledPageUrl", url);
}

ServiceWorkerMetrics::Site SiteFromURL(const GURL& gurl) {
  // UIThreadSearchTermsData::GoogleBaseURLValue() returns the google base
  // URL, but not available in content layer.
  static const char google_like_scope_prefix[] = "https://www.google.";
  static const char ntp_scope_path[] = "/_/chrome/";
  if (base::StartsWith(gurl.spec(), google_like_scope_prefix,
                       base::CompareCase::INSENSITIVE_ASCII) &&
      base::StartsWith(gurl.path(), ntp_scope_path,
                       base::CompareCase::SENSITIVE)) {
    return ServiceWorkerMetrics::Site::NEW_TAB_PAGE;
  }

  return ServiceWorkerMetrics::Site::OTHER;
}

enum EventHandledRatioType {
  EVENT_HANDLED_NONE,
  EVENT_HANDLED_SOME,
  EVENT_HANDLED_ALL,
  NUM_EVENT_HANDLED_RATIO_TYPE,
};

}  // namespace

bool ServiceWorkerMetrics::ShouldExcludeSiteFromHistogram(Site site) {
  return site == ServiceWorkerMetrics::Site::NEW_TAB_PAGE;
}

bool ServiceWorkerMetrics::ShouldExcludeURLFromHistogram(const GURL& url) {
  return ShouldExcludeSiteFromHistogram(SiteFromURL(url));
}

void ServiceWorkerMetrics::CountInitDiskCacheResult(bool result) {
  UMA_HISTOGRAM_BOOLEAN("ServiceWorker.DiskCache.InitResult", result);
}

void ServiceWorkerMetrics::CountReadResponseResult(
    ServiceWorkerMetrics::ReadResponseResult result) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.DiskCache.ReadResponseResult",
                            result, NUM_READ_RESPONSE_RESULT_TYPES);
}

void ServiceWorkerMetrics::CountWriteResponseResult(
    ServiceWorkerMetrics::WriteResponseResult result) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.DiskCache.WriteResponseResult",
                            result, NUM_WRITE_RESPONSE_RESULT_TYPES);
}

void ServiceWorkerMetrics::CountOpenDatabaseResult(
    ServiceWorkerDatabase::Status status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.OpenResult",
                            status, ServiceWorkerDatabase::STATUS_ERROR_MAX);
}

void ServiceWorkerMetrics::CountReadDatabaseResult(
    ServiceWorkerDatabase::Status status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.ReadResult",
                            status, ServiceWorkerDatabase::STATUS_ERROR_MAX);
}

void ServiceWorkerMetrics::CountWriteDatabaseResult(
    ServiceWorkerDatabase::Status status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.WriteResult",
                            status, ServiceWorkerDatabase::STATUS_ERROR_MAX);
}

void ServiceWorkerMetrics::RecordDestroyDatabaseResult(
    ServiceWorkerDatabase::Status status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.DestroyDatabaseResult",
                            status, ServiceWorkerDatabase::STATUS_ERROR_MAX);
}

void ServiceWorkerMetrics::RecordPurgeResourceResult(int net_error) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("ServiceWorker.Storage.PurgeResourceResult",
                              std::abs(net_error));
}

void ServiceWorkerMetrics::RecordDeleteAndStartOverResult(
    DeleteAndStartOverResult result) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Storage.DeleteAndStartOverResult",
                            result, NUM_DELETE_AND_START_OVER_RESULT_TYPES);
}

void ServiceWorkerMetrics::CountControlledPageLoad(const GURL& url) {
  Site site = SiteFromURL(url);
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.PageLoad", static_cast<int>(site),
                            static_cast<int>(Site::NUM_TYPES));

  if (ShouldExcludeSiteFromHistogram(site))
    return;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&RecordURLMetricOnUI, url));
}

void ServiceWorkerMetrics::RecordStartWorkerStatus(
    ServiceWorkerStatusCode status,
    bool is_installed) {
  if (is_installed) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.Status", status,
                              SERVICE_WORKER_ERROR_MAX_VALUE);
  } else {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartNewWorker.Status", status,
                              SERVICE_WORKER_ERROR_MAX_VALUE);
  }
}

void ServiceWorkerMetrics::RecordStartWorkerTime(const base::TimeDelta& time,
                                                 bool is_installed) {
  if (is_installed)
    UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.StartWorker.Time", time);
  else
    UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.StartNewWorker.Time", time);
}

void ServiceWorkerMetrics::RecordWorkerStopped(StopStatus status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.WorkerStopped",
                            static_cast<int>(status),
                            static_cast<int>(StopStatus::NUM_TYPES));
}

void ServiceWorkerMetrics::RecordStopWorkerTime(const base::TimeDelta& time) {
  UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.StopWorker.Time", time);
}

void ServiceWorkerMetrics::RecordActivateEventStatus(
    ServiceWorkerStatusCode status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.ActivateEventStatus", status,
                            SERVICE_WORKER_ERROR_MAX_VALUE);
}

void ServiceWorkerMetrics::RecordInstallEventStatus(
    ServiceWorkerStatusCode status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.InstallEventStatus", status,
                            SERVICE_WORKER_ERROR_MAX_VALUE);
}

void ServiceWorkerMetrics::RecordEventHandledRatio(EventType event,
                                                   size_t handled_events,
                                                   size_t fired_events) {
  if (!fired_events)
    return;
  EventHandledRatioType type = EVENT_HANDLED_SOME;
  if (fired_events == handled_events)
    type = EVENT_HANDLED_ALL;
  else if (handled_events == 0)
    type = EVENT_HANDLED_NONE;

  // For now Fetch is the only type that is recorded.
  if (event != EventType::FETCH)
    return;
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.EventHandledRatioType.Fetch", type,
                            NUM_EVENT_HANDLED_RATIO_TYPE);
}

void ServiceWorkerMetrics::RecordEventTimeout(EventType event) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.RequestTimeouts.Count",
                            static_cast<int>(event),
                            static_cast<int>(EventType::NUM_TYPES));
}

void ServiceWorkerMetrics::RecordEventDuration(EventType event,
                                               const base::TimeDelta& time) {
  switch (event) {
    case EventType::ACTIVATE:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.ActivateEvent.Time", time);
      break;
    case EventType::INSTALL:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.InstallEvent.Time", time);
      break;
    case EventType::SYNC:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.BackgroundSyncEvent.Time",
                                 time);
      break;
    case EventType::NOTIFICATION_CLICK:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.NotificationClickEvent.Time",
                                 time);
      break;
    case EventType::PUSH:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.PushEvent.Time", time);
      break;

    // Event duration for fetch is recorded separately.
    case EventType::FETCH:
    // For now event duration for these events is not recorded.
    case EventType::GEOFENCING:
    case EventType::SERVICE_PORT_CONNECT:
      break;

    case EventType::NUM_TYPES:
      NOTREACHED() << "Invalid event type";
      break;
  }
}

void ServiceWorkerMetrics::RecordFetchEventStatus(
    bool is_main_resource,
    ServiceWorkerStatusCode status) {
  if (is_main_resource) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.FetchEvent.MainResource.Status",
                              status, SERVICE_WORKER_ERROR_MAX_VALUE);
  } else {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.FetchEvent.Subresource.Status",
                              status, SERVICE_WORKER_ERROR_MAX_VALUE);
  }
}

void ServiceWorkerMetrics::RecordFetchEventTime(
    ServiceWorkerFetchEventResult result,
    const base::TimeDelta& time) {
  switch (result) {
    case ServiceWorkerFetchEventResult::
        SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.FetchEvent.Fallback.Time",
                                 time);
      break;
    case ServiceWorkerFetchEventResult::
        SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.FetchEvent.HasResponse.Time",
                                 time);
      break;
  }
}

void ServiceWorkerMetrics::RecordURLRequestJobResult(
    bool is_main_resource,
    URLRequestJobResult result) {
  if (is_main_resource) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.URLRequestJob.MainResource.Result",
                              result, NUM_REQUEST_JOB_RESULT_TYPES);
  } else {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.URLRequestJob.Subresource.Result",
                              result, NUM_REQUEST_JOB_RESULT_TYPES);
  }
}

void ServiceWorkerMetrics::RecordStatusZeroResponseError(
    bool is_main_resource,
    blink::WebServiceWorkerResponseError error) {
  if (is_main_resource) {
    UMA_HISTOGRAM_ENUMERATION(
        "ServiceWorker.URLRequestJob.MainResource.StatusZeroError", error,
        blink::WebServiceWorkerResponseErrorLast + 1);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "ServiceWorker.URLRequestJob.Subresource.StatusZeroError", error,
        blink::WebServiceWorkerResponseErrorLast + 1);
  }
}

void ServiceWorkerMetrics::RecordFallbackedRequestMode(FetchRequestMode mode) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.URLRequestJob.FallbackedRequestMode",
                            mode, FETCH_REQUEST_MODE_LAST + 1);
}

void ServiceWorkerMetrics::RecordTimeBetweenEvents(
    const base::TimeDelta& time) {
  UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.TimeBetweenEvents", time);
}

}  // namespace content
