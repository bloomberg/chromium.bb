// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_metrics.h"

#include <limits>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_util.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

namespace {

std::string StartSituationToSuffix(
    ServiceWorkerMetrics::StartSituation situation) {
  switch (situation) {
    case ServiceWorkerMetrics::StartSituation::DURING_STARTUP:
      return "_DuringStartup";
    case ServiceWorkerMetrics::StartSituation::NEW_PROCESS:
      return "_NewProcess";
    case ServiceWorkerMetrics::StartSituation::EXISTING_PROCESS:
      return "_ExistingProcess";
    default:
      NOTREACHED() << static_cast<int>(situation);
  }
  return "_Unknown";
}

// Use this for histograms with dynamically generated names, which
// otherwise can't use the UMA_HISTOGRAM macro without code duplication.
void RecordSuffixedTimeHistogram(const std::string& name,
                                 const std::string& suffix,
                                 base::TimeDelta sample) {
  const std::string name_with_suffix = name + suffix;
  // This unrolls UMA_HISTOGRAM_MEDIUM_TIMES.
  base::HistogramBase* histogram_pointer = base::Histogram::FactoryTimeGet(
      name_with_suffix, base::TimeDelta::FromMilliseconds(10),
      base::TimeDelta::FromMinutes(3), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram_pointer->AddTime(sample);
}

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

const char* ServiceWorkerMetrics::EventTypeToString(EventType event_type) {
  switch (event_type) {
    case EventType::ACTIVATE:
      return "Activate";
    case EventType::INSTALL:
      return "Install";
    case EventType::SYNC:
      return "Sync";
    case EventType::NOTIFICATION_CLICK:
      return "Notification Click";
    case EventType::NOTIFICATION_CLOSE:
      return "Notification Close";
    case EventType::PUSH:
      return "Push";
    case EventType::GEOFENCING:
      return "Geofencing";
    case EventType::MESSAGE:
      return "Message";
    case EventType::FETCH_MAIN_FRAME:
      return "Fetch Main Frame";
    case EventType::FETCH_SUB_FRAME:
      return "Fetch Sub Frame";
    case EventType::FETCH_SHARED_WORKER:
      return "Fetch Shared Worker";
    case EventType::FETCH_SUB_RESOURCE:
      return "Fetch Subresource";
    case EventType::UNKNOWN:
      return "Unknown";
    case EventType::FOREIGN_FETCH:
      return "Foreign Fetch";
    case EventType::DEPRECATED_FETCH:
    case EventType::NUM_TYPES:
      break;
  }
  NOTREACHED() << "Got unexpected event type: " << static_cast<int>(event_type);
  return "error";
}

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
    EventType purpose,
    bool is_installed) {
  if (is_installed) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.Status", status,
                              SERVICE_WORKER_ERROR_MAX_VALUE);
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.Purpose",
                              static_cast<int>(purpose),
                              static_cast<int>(EventType::NUM_TYPES));
    if (status == SERVICE_WORKER_ERROR_TIMEOUT) {
      UMA_HISTOGRAM_ENUMERATION(
          "ServiceWorker.StartWorker.Timeout.StartPurpose",
          static_cast<int>(purpose), static_cast<int>(EventType::NUM_TYPES));
    }
  } else {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartNewWorker.Status", status,
                              SERVICE_WORKER_ERROR_MAX_VALUE);
  }
}

void ServiceWorkerMetrics::RecordStartWorkerTime(
    base::TimeDelta time,
    bool is_installed,
    StartSituation start_situation) {
  if (is_installed) {
    std::string name = "ServiceWorker.StartWorker.Time";
    UMA_HISTOGRAM_MEDIUM_TIMES(name, time);
    RecordSuffixedTimeHistogram(name, StartSituationToSuffix(start_situation),
                                time);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.StartNewWorker.Time", time);
  }
}

void ServiceWorkerMetrics::RecordWorkerStopped(StopStatus status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.WorkerStopped",
                            static_cast<int>(status),
                            static_cast<int>(StopStatus::NUM_TYPES));
}

void ServiceWorkerMetrics::RecordStopWorkerTime(base::TimeDelta time) {
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

  // For now Fetch and Foreign Fetch are the only types that are recorded.
  switch (event) {
    case EventType::FETCH_MAIN_FRAME:
    case EventType::FETCH_SUB_FRAME:
    case EventType::FETCH_SHARED_WORKER:
    case EventType::FETCH_SUB_RESOURCE:
      UMA_HISTOGRAM_ENUMERATION("ServiceWorker.EventHandledRatioType.Fetch",
                                type, NUM_EVENT_HANDLED_RATIO_TYPE);
      break;
    case EventType::FOREIGN_FETCH:
      UMA_HISTOGRAM_ENUMERATION(
          "ServiceWorker.EventHandledRatioType.ForeignFetch", type,
          NUM_EVENT_HANDLED_RATIO_TYPE);
      break;
    default:
      // Do nothing.
      break;
  }
}

void ServiceWorkerMetrics::RecordEventTimeout(EventType event) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.RequestTimeouts.Count",
                            static_cast<int>(event),
                            static_cast<int>(EventType::NUM_TYPES));
}

void ServiceWorkerMetrics::RecordEventDuration(EventType event,
                                               base::TimeDelta time,
                                               bool was_handled) {
  switch (event) {
    case EventType::ACTIVATE:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.ActivateEvent.Time", time);
      break;
    case EventType::INSTALL:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.InstallEvent.Time", time);
      break;
    case EventType::FETCH_MAIN_FRAME:
    case EventType::FETCH_SUB_FRAME:
    case EventType::FETCH_SHARED_WORKER:
    case EventType::FETCH_SUB_RESOURCE:
      if (was_handled) {
        UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.FetchEvent.HasResponse.Time",
                                   time);
      } else {
        UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.FetchEvent.Fallback.Time",
                                   time);
      }
      break;
    case EventType::FOREIGN_FETCH:
      if (was_handled) {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "ServiceWorker.ForeignFetchEvent.HasResponse.Time", time);
      } else {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "ServiceWorker.ForeignFetchEvent.Fallback.Time", time);
      }
      break;
    case EventType::SYNC:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.BackgroundSyncEvent.Time",
                                 time);
      break;
    case EventType::NOTIFICATION_CLICK:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.NotificationClickEvent.Time",
                                 time);
      break;
    case EventType::NOTIFICATION_CLOSE:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.NotificationCloseEvent.Time",
                                 time);
      break;
    case EventType::PUSH:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.PushEvent.Time", time);
      break;
    case EventType::MESSAGE:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.ExtendableMessageEvent.Time",
                                 time);
      break;

    // For now event duration for these events is not recorded.
    case EventType::GEOFENCING:
      break;

    case EventType::DEPRECATED_FETCH:
    case EventType::UNKNOWN:
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

void ServiceWorkerMetrics::RecordTimeBetweenEvents(base::TimeDelta time) {
  UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.TimeBetweenEvents", time);
}

void ServiceWorkerMetrics::RecordProcessCreated(bool is_new_process) {
  UMA_HISTOGRAM_BOOLEAN("EmbeddedWorkerInstance.ProcessCreated",
                        is_new_process);
}

void ServiceWorkerMetrics::RecordTimeToSendStartWorker(
    base::TimeDelta duration,
    StartSituation situation) {
  std::string name = "EmbeddedWorkerInstance.Start.TimeToSendStartWorker";
  UMA_HISTOGRAM_MEDIUM_TIMES(name, duration);
  RecordSuffixedTimeHistogram(name, StartSituationToSuffix(situation),
                              duration);
}

void ServiceWorkerMetrics::RecordTimeToURLJob(base::TimeDelta duration,
                                              StartSituation situation) {
  std::string name = "EmbeddedWorkerInstance.Start.TimeToURLJob";
  UMA_HISTOGRAM_MEDIUM_TIMES(name, duration);
  RecordSuffixedTimeHistogram(name, StartSituationToSuffix(situation),
                              duration);
}

void ServiceWorkerMetrics::RecordTimeToLoad(base::TimeDelta duration,
                                            LoadSource source,
                                            StartSituation situation) {
  std::string name;
  switch (source) {
    case LoadSource::NETWORK:
      name = "EmbeddedWorkerInstance.Start.TimeToLoad.Network";
      UMA_HISTOGRAM_MEDIUM_TIMES(name, duration);
      RecordSuffixedTimeHistogram(name, StartSituationToSuffix(situation),
                                  duration);
      break;
    case LoadSource::HTTP_CACHE:
      name = "EmbeddedWorkerInstance.Start.TimeToLoad.HttpCache";
      UMA_HISTOGRAM_MEDIUM_TIMES(name, duration);
      RecordSuffixedTimeHistogram(name, StartSituationToSuffix(situation),
                                  duration);
      break;
    case LoadSource::SERVICE_WORKER_STORAGE:
      name = "EmbeddedWorkerInstance.Start.TimeToLoad.InstalledScript";
      UMA_HISTOGRAM_MEDIUM_TIMES(name, duration);
      RecordSuffixedTimeHistogram(name, StartSituationToSuffix(situation),
                                  duration);
      break;
    default:
      NOTREACHED() << static_cast<int>(source);
  }
}

void ServiceWorkerMetrics::RecordTimeToStartThread(base::TimeDelta duration,
                                                   StartSituation situation) {
  std::string name = "EmbeddedWorkerInstance.Start.TimeToStartThread";
  UMA_HISTOGRAM_MEDIUM_TIMES(name, duration);
  RecordSuffixedTimeHistogram(name, StartSituationToSuffix(situation),
                              duration);
}

void ServiceWorkerMetrics::RecordTimeToEvaluateScript(
    base::TimeDelta duration,
    StartSituation situation) {
  std::string name = "EmbeddedWorkerInstance.Start.TimeToEvaluateScript";
  UMA_HISTOGRAM_MEDIUM_TIMES(name, duration);
  RecordSuffixedTimeHistogram(name, StartSituationToSuffix(situation),
                              duration);
}

const char* ServiceWorkerMetrics::LoadSourceToString(LoadSource source) {
  switch (source) {
    case LoadSource::NETWORK:
      return "Network";
    case LoadSource::HTTP_CACHE:
      return "HTTP cache";
    case LoadSource::SERVICE_WORKER_STORAGE:
      return "Service worker storage";
  }
  NOTREACHED() << static_cast<int>(source);
  return nullptr;
}

ServiceWorkerMetrics::StartSituation ServiceWorkerMetrics::GetStartSituation(
    bool is_browser_startup_complete,
    bool is_new_process) {
  if (!is_browser_startup_complete)
    return StartSituation::DURING_STARTUP;
  if (is_new_process)
    return StartSituation::NEW_PROCESS;
  return StartSituation::EXISTING_PROCESS;
}

void ServiceWorkerMetrics::RecordStartStatusAfterFailure(
    int failure_count,
    ServiceWorkerStatusCode status) {
  DCHECK_GT(failure_count, 0);

  if (status == SERVICE_WORKER_OK) {
    UMA_HISTOGRAM_COUNTS_1000("ServiceWorker.StartWorker.FailureStreakEnded",
                              failure_count);
  } else if (failure_count < std::numeric_limits<int>::max()) {
    UMA_HISTOGRAM_COUNTS_1000("ServiceWorker.StartWorker.FailureStreak",
                              failure_count + 1);
  }

  if (failure_count == 1) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.AfterFailureStreak_1",
                              status, SERVICE_WORKER_ERROR_MAX_VALUE);
  } else if (failure_count == 2) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.AfterFailureStreak_2",
                              status, SERVICE_WORKER_ERROR_MAX_VALUE);
  } else if (failure_count == 3) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.AfterFailureStreak_3",
                              status, SERVICE_WORKER_ERROR_MAX_VALUE);
  }
}

}  // namespace content
