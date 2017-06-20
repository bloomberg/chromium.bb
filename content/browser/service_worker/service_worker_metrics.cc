// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_metrics.h"

#include <limits>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

namespace {

std::string StartSituationToSuffix(
    ServiceWorkerMetrics::StartSituation situation) {
  // Don't change these returned strings. They are written (in hashed form) into
  // logs.
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

std::string EventTypeToSuffix(ServiceWorkerMetrics::EventType event_type) {
  // Don't change these returned strings. They are written (in hashed form) into
  // logs.
  switch (event_type) {
    case ServiceWorkerMetrics::EventType::ACTIVATE:
      return "_ACTIVATE";
    case ServiceWorkerMetrics::EventType::INSTALL:
      return "_INSTALL";
    case ServiceWorkerMetrics::EventType::SYNC:
      return "_SYNC";
    case ServiceWorkerMetrics::EventType::NOTIFICATION_CLICK:
      return "_NOTIFICATION_CLICK";
    case ServiceWorkerMetrics::EventType::PUSH:
      return "_PUSH";
    case ServiceWorkerMetrics::EventType::MESSAGE:
      return "_MESSAGE";
    case ServiceWorkerMetrics::EventType::NOTIFICATION_CLOSE:
      return "_NOTIFICATION_CLOSE";
    case ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME:
      return "_FETCH_MAIN_FRAME";
    case ServiceWorkerMetrics::EventType::FETCH_SUB_FRAME:
      return "_FETCH_SUB_FRAME";
    case ServiceWorkerMetrics::EventType::FETCH_SHARED_WORKER:
      return "_FETCH_SHARED_WORKER";
    case ServiceWorkerMetrics::EventType::FETCH_SUB_RESOURCE:
      return "_FETCH_SUB_RESOURCE";
    case ServiceWorkerMetrics::EventType::UNKNOWN:
      return "_UNKNOWN";
    case ServiceWorkerMetrics::EventType::FOREIGN_FETCH:
      return "_FOREIGN_FETCH";
    case ServiceWorkerMetrics::EventType::FETCH_WAITUNTIL:
      return "_FETCH_WAITUNTIL";
    case ServiceWorkerMetrics::EventType::FOREIGN_FETCH_WAITUNTIL:
      return "_FOREIGN_FETCH_WAITUNTIL";
    case ServiceWorkerMetrics::EventType::EXTERNAL_REQUEST:
      return "_EXTERNAL_REQUEST";
    case ServiceWorkerMetrics::EventType::PAYMENT_REQUEST:
      return "_PAYMENT_REQUEST";
    case ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_ABORT:
      return "_BACKGROUND_FETCH_ABORT";
    case ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_CLICK:
      return "_BACKGROUND_FETCH_CLICK";
    case ServiceWorkerMetrics::EventType::BACKGROUND_FETCH_FAIL:
      return "_BACKGROUND_FETCH_FAIL";
    case ServiceWorkerMetrics::EventType::BACKGROUND_FETCHED:
      return "_BACKGROUND_FETCHED";
    case ServiceWorkerMetrics::EventType::NAVIGATION_HINT:
      return "_NAVIGATION_HINT";
    case ServiceWorkerMetrics::EventType::NUM_TYPES:
      NOTREACHED() << static_cast<int>(event_type);
  }
  return "_UNKNOWN";
}

ServiceWorkerMetrics::WorkerPreparationType GetWorkerPreparationType(
    EmbeddedWorkerStatus initial_worker_status,
    ServiceWorkerMetrics::StartSituation start_situation) {
  using Situation = ServiceWorkerMetrics::StartSituation;
  using Preparation = ServiceWorkerMetrics::WorkerPreparationType;
  switch (initial_worker_status) {
    case EmbeddedWorkerStatus::STOPPED: {
      switch (start_situation) {
        case Situation::DURING_STARTUP:
          return Preparation::START_DURING_STARTUP;
        case Situation::NEW_PROCESS:
          return Preparation::START_IN_NEW_PROCESS;
        case Situation::EXISTING_PROCESS:
          return Preparation::START_IN_EXISTING_PROCESS;
        case Situation::UNKNOWN:
          break;
      }
      break;
    }
    case EmbeddedWorkerStatus::STARTING:
      return Preparation::STARTING;
    case EmbeddedWorkerStatus::RUNNING:
      return Preparation::RUNNING;
    case EmbeddedWorkerStatus::STOPPING:
      return Preparation::STOPPING;
  }
  NOTREACHED() << static_cast<int>(initial_worker_status);
  return Preparation::UNKNOWN;
}

std::string GetWorkerPreparationSuffix(
    ServiceWorkerMetrics::WorkerPreparationType status) {
  using Preparation = ServiceWorkerMetrics::WorkerPreparationType;
  switch (status) {
    case Preparation::UNKNOWN:
    case Preparation::NUM_TYPES:
      break;
    case Preparation::START_DURING_STARTUP:
      return "_StartWorkerDuringStartup";
    case Preparation::START_IN_NEW_PROCESS:
      return "_StartWorkerNewProcess";
    case Preparation::START_IN_EXISTING_PROCESS:
      return "_StartWorkerExistingProcess";
    case Preparation::STARTING:
      return "_StartingWorker";
    case Preparation::RUNNING:
      return "_RunningWorker";
    case Preparation::STOPPING:
      return "_StoppingWorker";
  }
  NOTREACHED();
  return "_UNKNOWN";
}

std::string GetSiteSuffix(ServiceWorkerMetrics::Site site) {
  switch (site) {
    case ServiceWorkerMetrics::Site::OTHER:
    case ServiceWorkerMetrics::Site::WITH_FETCH_HANDLER:
    case ServiceWorkerMetrics::Site::WITHOUT_FETCH_HANDLER:
      return "";
    case ServiceWorkerMetrics::Site::NEW_TAB_PAGE:
      return ".ntp";
    case ServiceWorkerMetrics::Site::PLUS:
      return ".plus";
    case ServiceWorkerMetrics::Site::INBOX:
      return ".inbox";
    case ServiceWorkerMetrics::Site::DOCS:
      return ".docs";
    case ServiceWorkerMetrics::Site::NUM_TYPES:
      NOTREACHED() << static_cast<int>(site);
  }
  NOTREACHED();
  return "";
}

// Use this for histograms with dynamically generated names, which
// otherwise can't use the UMA_HISTOGRAM macro without code duplication.
void RecordSuffixedTimeHistogram(const std::string& name,
                                 const std::string& suffix,
                                 base::TimeDelta sample) {
  const std::string name_with_suffix = name + suffix;
  // This unrolls UMA_HISTOGRAM_TIMES.
  base::HistogramBase* histogram_pointer = base::Histogram::FactoryTimeGet(
      name_with_suffix, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromSeconds(10), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram_pointer->AddTime(sample);
}

// Use this for histograms with dynamically generated names, which
// otherwise can't use the UMA_MEDIUM_HISTOGRAM macro without code duplication.
void RecordSuffixedMediumTimeHistogram(const std::string& name,
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

// Use this for histograms with dynamically generated names, which
// otherwise can't use the UMA_HISTOGRAM macro without code duplication.
void RecordSuffixedStatusHistogram(const std::string& name,
                                   const std::string& suffix,
                                   ServiceWorkerStatusCode status) {
  const std::string name_with_suffix = name + suffix;
  // This unrolls UMA_HISTOGRAM_ENUMERATION.
  base::HistogramBase* histogram_pointer = base::LinearHistogram::FactoryGet(
      name_with_suffix, 1, SERVICE_WORKER_ERROR_MAX_VALUE,
      SERVICE_WORKER_ERROR_MAX_VALUE + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram_pointer->Add(status);
}

void RecordURLMetricOnUI(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetContentClient()->browser()->RecordURLMetric(
      "ServiceWorker.ControlledPageUrl", url);
}

enum EventHandledRatioType {
  EVENT_HANDLED_NONE,
  EVENT_HANDLED_SOME,
  EVENT_HANDLED_ALL,
  NUM_EVENT_HANDLED_RATIO_TYPE,
};

}  // namespace

using ScopedEventRecorder = ServiceWorkerMetrics::ScopedEventRecorder;

ScopedEventRecorder::ScopedEventRecorder() {}

ScopedEventRecorder::~ScopedEventRecorder() {
  for (const auto& ev : event_stats_) {
    RecordEventHandledRatio(ev.first, ev.second.handled_events,
                            ev.second.fired_events);
  }
}

void ScopedEventRecorder::RecordEventHandledStatus(
    ServiceWorkerMetrics::EventType event,
    bool handled) {
  event_stats_[event].fired_events++;
  if (handled)
    event_stats_[event].handled_events++;
}

void ScopedEventRecorder::RecordEventHandledRatio(
    ServiceWorkerMetrics::EventType event,
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
    case EventType::FETCH_WAITUNTIL:
      return "Fetch WaitUntil";
    case EventType::FOREIGN_FETCH_WAITUNTIL:
      return "Foreign Fetch WaitUntil";
    case EventType::EXTERNAL_REQUEST:
      return "External Request";
    case EventType::PAYMENT_REQUEST:
      return "Payment Request";
    case EventType::BACKGROUND_FETCH_ABORT:
      return "Background Fetch Abort";
    case EventType::BACKGROUND_FETCH_CLICK:
      return "Background Fetch Click";
    case EventType::BACKGROUND_FETCH_FAIL:
      return "Background Fetch Fail";
    case EventType::BACKGROUND_FETCHED:
      return "Background Fetched";
    case EventType::NAVIGATION_HINT:
      return "Navigation Hint";
    case EventType::NUM_TYPES:
      break;
  }
  NOTREACHED() << "Got unexpected event type: " << static_cast<int>(event_type);
  return "error";
}

ServiceWorkerMetrics::Site ServiceWorkerMetrics::SiteFromURL(const GURL& url) {
  // UIThreadSearchTermsData::GoogleBaseURLValue() returns the google base
  // URL, but not available in content layer.
  static const char google_like_scope_prefix[] = "https://www.google.";
  static const char ntp_scope_path[] = "/_/chrome/";
  if (base::StartsWith(url.spec(), google_like_scope_prefix,
                       base::CompareCase::INSENSITIVE_ASCII) &&
      base::StartsWith(url.path(), ntp_scope_path,
                       base::CompareCase::SENSITIVE)) {
    return ServiceWorkerMetrics::Site::NEW_TAB_PAGE;
  }

  const std::string host = url.host();
  if (host == "plus.google.com")
    return ServiceWorkerMetrics::Site::PLUS;
  if (host == "inbox.google.com")
    return ServiceWorkerMetrics::Site::INBOX;
  if ((host == "docs.google.com") || (host == "drive.google.com"))
    return ServiceWorkerMetrics::Site::DOCS;
  return ServiceWorkerMetrics::Site::OTHER;
}

bool ServiceWorkerMetrics::ShouldExcludeSiteFromHistogram(Site site) {
  return site == ServiceWorkerMetrics::Site::NEW_TAB_PAGE;
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

void ServiceWorkerMetrics::CountControlledPageLoad(
    Site site,
    const GURL& url,
    bool is_main_frame_load,
    ui::PageTransition page_transition,
    size_t redirect_chain_length) {
  DCHECK_NE(site, Site::OTHER);
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.PageLoad", static_cast<int>(site),
                            static_cast<int>(Site::NUM_TYPES));
  if (is_main_frame_load) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.MainFramePageLoad",
                              static_cast<int>(site),
                              static_cast<int>(Site::NUM_TYPES));
  }
  if (ShouldExcludeSiteFromHistogram(site))
    return;

  if (is_main_frame_load) {
    UMA_HISTOGRAM_ENUMERATION(
        "ServiceWorker.MainFramePageLoad.CoreTransition",
        static_cast<int>(ui::PageTransitionStripQualifier(page_transition)),
        static_cast<int>(ui::PAGE_TRANSITION_LAST_CORE) + 1);
    // Currently the max number of HTTP redirects is 20 which is defined as
    // kMaxRedirects in net/url_request/url_request.cc. So the max number of the
    // chain length is 21.
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ServiceWorker.MainFramePageLoad.RedirectChainLength",
        redirect_chain_length, 21);
  }
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&RecordURLMetricOnUI, url));
}

void ServiceWorkerMetrics::RecordStartWorkerStatus(
    ServiceWorkerStatusCode status,
    EventType purpose,
    bool is_installed) {
  if (!is_installed) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartNewWorker.Status", status,
                              SERVICE_WORKER_ERROR_MAX_VALUE);
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.Status", status,
                            SERVICE_WORKER_ERROR_MAX_VALUE);
  RecordSuffixedStatusHistogram("ServiceWorker.StartWorker.StatusByPurpose",
                                EventTypeToSuffix(purpose), status);
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.Purpose",
                            static_cast<int>(purpose),
                            static_cast<int>(EventType::NUM_TYPES));
  if (status == SERVICE_WORKER_ERROR_TIMEOUT) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.Timeout.StartPurpose",
                              static_cast<int>(purpose),
                              static_cast<int>(EventType::NUM_TYPES));
  }
}

void ServiceWorkerMetrics::RecordStartWorkerTime(base::TimeDelta time,
                                                 bool is_installed,
                                                 StartSituation start_situation,
                                                 EventType purpose) {
  if (is_installed) {
    UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.StartWorker.Time", time);
    RecordSuffixedMediumTimeHistogram("ServiceWorker.StartWorker.Time",
                                      StartSituationToSuffix(start_situation),
                                      time);
    RecordSuffixedMediumTimeHistogram(
        "ServiceWorker.StartWorker.Time",
        StartSituationToSuffix(start_situation) + EventTypeToSuffix(purpose),
        time);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.StartNewWorker.Time", time);
  }
}

void ServiceWorkerMetrics::RecordActivatedWorkerPreparationForMainFrame(
    base::TimeDelta time,
    EmbeddedWorkerStatus initial_worker_status,
    StartSituation start_situation,
    bool did_navigation_preload) {
  // Record the worker preparation type.
  WorkerPreparationType preparation =
      GetWorkerPreparationType(initial_worker_status, start_situation);
  UMA_HISTOGRAM_ENUMERATION(
      "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Type",
      static_cast<int>(preparation),
      static_cast<int>(WorkerPreparationType::NUM_TYPES));
  if (did_navigation_preload) {
    // TODO(falken): Consider removing this UMA if it turns out the same as
    // ServiceWorker.NavPreload.WorkerPreparationType. That UMA is logged at
    // the same time as the other NavPreload metrics (which requires both the
    // worker to start and the nav preload response to arrive successfuly), so
    // they are more safely compared together.
    UMA_HISTOGRAM_ENUMERATION(
        "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Type_"
        "NavigationPreloadEnabled",
        static_cast<int>(preparation),
        static_cast<int>(WorkerPreparationType::NUM_TYPES));
  }

  // Record the preparation time.
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Time", time);

  // Record the preparation time using the worker preparation suffix.
  RecordSuffixedMediumTimeHistogram(
      "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Time",
      GetWorkerPreparationSuffix(preparation), time);

  // Record the preparation time using the navigation preload suffix.
  if (did_navigation_preload) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Time_"
        "NavigationPreloadEnabled",
        time);
    // We're mostly interested in when the worker needed to start up.
    if (initial_worker_status != EmbeddedWorkerStatus::RUNNING) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "ServiceWorker.ActivatedWorkerPreparationForMainFrame.Time_"
          "WorkerStartOccurred_NavigationPreloadEnabled",
          time);
    }
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
    ServiceWorkerStatusCode status,
    bool is_shutdown) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.ActivateEventStatus", status,
                            SERVICE_WORKER_ERROR_MAX_VALUE);
  if (is_shutdown) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.ActivateEventStatus_InShutdown",
                              status, SERVICE_WORKER_ERROR_MAX_VALUE);
  } else {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.ActivateEventStatus_NotInShutdown",
                              status, SERVICE_WORKER_ERROR_MAX_VALUE);
  }
}

void ServiceWorkerMetrics::RecordInstallEventStatus(
    ServiceWorkerStatusCode status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.InstallEventStatus", status,
                            SERVICE_WORKER_ERROR_MAX_VALUE);
}

void ServiceWorkerMetrics::RecordForeignFetchRegistrationCount(
    size_t scope_count,
    size_t origin_count) {
  UMA_HISTOGRAM_COUNTS_100("ServiceWorker.ForeignFetch.ScopeCount",
                           scope_count);
  if (scope_count > 0) {
    // Only record number of origins if service worker registered for at least
    // one foreign fetch scope.
    UMA_HISTOGRAM_COUNTS_100("ServiceWorker.ForeignFetch.OriginCount",
                             origin_count);
  }
}

void ServiceWorkerMetrics::RecordEventDispatchingDelay(EventType event_type,
                                                       base::TimeDelta time,
                                                       Site site_for_metrics) {
  const std::string name = "ServiceWorker.EventDispatchingDelay";
  UMA_HISTOGRAM_TIMES(name, time);
  const std::string event_type_suffix = EventTypeToSuffix(event_type);
  const std::string site_suffix = GetSiteSuffix(site_for_metrics);
  RecordSuffixedTimeHistogram(name, event_type_suffix + site_suffix, time);
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
    case EventType::FETCH_WAITUNTIL:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.FetchEvent.WaitUntil.Time",
                                 time);
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
    case EventType::FOREIGN_FETCH_WAITUNTIL:
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "ServiceWorker.ForeignFetchEvent.WaitUntil.Time", time);
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
    case EventType::EXTERNAL_REQUEST:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.ExternalRequest.Time", time);
      break;
    case EventType::PAYMENT_REQUEST:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.PaymentRequestEvent.Time",
                                 time);
      break;
    case EventType::BACKGROUND_FETCH_ABORT:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.BackgroundFetchAbortEvent.Time",
                                 time);
      break;
    case EventType::BACKGROUND_FETCH_CLICK:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.BackgroundFetchClickEvent.Time",
                                 time);
      break;
    case EventType::BACKGROUND_FETCH_FAIL:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.BackgroundFetchFailEvent.Time",
                                 time);
      break;
    case EventType::BACKGROUND_FETCHED:
      UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.BackgroundFetchedEvent.Time",
                                 time);
      break;

    case EventType::NAVIGATION_HINT:
    // The navigation hint should not be sent as an event.
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
        blink::kWebServiceWorkerResponseErrorLast + 1);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "ServiceWorker.URLRequestJob.Subresource.StatusZeroError", error,
        blink::kWebServiceWorkerResponseErrorLast + 1);
  }
}

void ServiceWorkerMetrics::RecordFallbackedRequestMode(FetchRequestMode mode) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.URLRequestJob.FallbackedRequestMode",
                            mode, FETCH_REQUEST_MODE_LAST + 1);
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
  RecordSuffixedMediumTimeHistogram(name, StartSituationToSuffix(situation),
                                    duration);
}

void ServiceWorkerMetrics::RecordTimeToURLJob(base::TimeDelta duration,
                                              StartSituation situation) {
  std::string name = "EmbeddedWorkerInstance.Start.TimeToURLJob";
  UMA_HISTOGRAM_MEDIUM_TIMES(name, duration);
  RecordSuffixedMediumTimeHistogram(name, StartSituationToSuffix(situation),
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
      RecordSuffixedMediumTimeHistogram(name, StartSituationToSuffix(situation),
                                        duration);
      break;
    case LoadSource::HTTP_CACHE:
      name = "EmbeddedWorkerInstance.Start.TimeToLoad.HttpCache";
      UMA_HISTOGRAM_MEDIUM_TIMES(name, duration);
      RecordSuffixedMediumTimeHistogram(name, StartSituationToSuffix(situation),
                                        duration);
      break;
    case LoadSource::SERVICE_WORKER_STORAGE:
      name = "EmbeddedWorkerInstance.Start.TimeToLoad.InstalledScript";
      UMA_HISTOGRAM_MEDIUM_TIMES(name, duration);
      RecordSuffixedMediumTimeHistogram(name, StartSituationToSuffix(situation),
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
  RecordSuffixedMediumTimeHistogram(name, StartSituationToSuffix(situation),
                                    duration);
}

void ServiceWorkerMetrics::RecordTimeToEvaluateScript(
    base::TimeDelta duration,
    StartSituation situation) {
  std::string name = "EmbeddedWorkerInstance.Start.TimeToEvaluateScript";
  UMA_HISTOGRAM_MEDIUM_TIMES(name, duration);
  RecordSuffixedMediumTimeHistogram(name, StartSituationToSuffix(situation),
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

void ServiceWorkerMetrics::RecordNavigationPreloadRequestHeaderSize(
    size_t size) {
  UMA_HISTOGRAM_COUNTS_100000("ServiceWorker.NavigationPreload.HeaderSize",
                              size);
}

void ServiceWorkerMetrics::RecordNavigationPreloadResponse(
    base::TimeDelta worker_start,
    base::TimeDelta response_start,
    EmbeddedWorkerStatus initial_worker_status,
    StartSituation start_situation,
    ResourceType resource_type) {
  DCHECK_GE(worker_start.ToInternalValue(), 0);
  DCHECK_GE(response_start.ToInternalValue(), 0);
  DCHECK(resource_type == RESOURCE_TYPE_MAIN_FRAME ||
         resource_type == RESOURCE_TYPE_SUB_FRAME);
  const bool is_main_frame = (resource_type == RESOURCE_TYPE_MAIN_FRAME);
  // TODO(falken): Log sub-frame navigations also.
  if (!is_main_frame) {
    return;
  }
  const bool nav_preload_finished_first = response_start < worker_start;
  const base::TimeDelta concurrent_time =
      nav_preload_finished_first ? response_start : worker_start;
  base::TimeDelta worker_wait_time;
  if (nav_preload_finished_first) {
    worker_wait_time = worker_start - response_start;
  }
  const bool worker_start_occurred =
      initial_worker_status != EmbeddedWorkerStatus::RUNNING;
  const WorkerPreparationType preparation =
      GetWorkerPreparationType(initial_worker_status, start_situation);

  UMA_HISTOGRAM_ENUMERATION(
      "ServiceWorker.NavPreload.WorkerPreparationType_MainFrame", preparation,
      WorkerPreparationType::NUM_TYPES);
  UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.NavPreload.ResponseTime_MainFrame",
                             response_start);
  UMA_HISTOGRAM_BOOLEAN("ServiceWorker.NavPreload.FinishedFirst_MainFrame",
                        nav_preload_finished_first);
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "ServiceWorker.NavPreload.ConcurrentTime_MainFrame", concurrent_time);
  if (nav_preload_finished_first) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "ServiceWorker.NavPreload.WorkerWaitTime_MainFrame", worker_wait_time);
  }

  if (worker_start_occurred) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "ServiceWorker.NavPreload.ResponseTime_MainFrame_"
        "WorkerStartOccurred",
        response_start);
    UMA_HISTOGRAM_BOOLEAN(
        "ServiceWorker.NavPreload.FinishedFirst_MainFrame_"
        "WorkerStartOccurred",
        nav_preload_finished_first);
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "ServiceWorker.NavPreload.ConcurrentTime_MainFrame_"
        "WorkerStartOccurred",
        concurrent_time);
    if (nav_preload_finished_first) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "ServiceWorker.NavPreload.WorkerWaitTime_MainFrame_"
          "WorkerStartOccurred",
          worker_wait_time);
    }
  }
}

void ServiceWorkerMetrics::RecordContextRequestHandlerStatus(
    ServiceWorkerContextRequestHandler::CreateJobStatus status,
    bool is_installed,
    bool is_main_script) {
  const int value = static_cast<int>(status);
  const int max = static_cast<int>(
      ServiceWorkerContextRequestHandler::CreateJobStatus::NUM_TYPES);
  if (is_installed) {
    if (is_main_script) {
      UMA_HISTOGRAM_ENUMERATION(
          "ServiceWorker.ContextRequestHandlerStatus.InstalledWorker."
          "MainScript",
          value, max);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "ServiceWorker.ContextRequestHandlerStatus.InstalledWorker."
          "ImportedScript",
          value, max);
    }
  } else {
    if (is_main_script) {
      UMA_HISTOGRAM_ENUMERATION(
          "ServiceWorker.ContextRequestHandlerStatus.NewWorker.MainScript",
          value, max);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "ServiceWorker.ContextRequestHandlerStatus.NewWorker."
          "ImportedScript",
          value, max);
    }
  }
}

void ServiceWorkerMetrics::RecordRuntime(base::TimeDelta time) {
  // Start at 1 second since we expect service worker to last at least this
  // long: the update timer and idle timeout timer run on the order of seconds.
  constexpr base::TimeDelta kMin = base::TimeDelta::FromSeconds(1);
  // End at 1 day since service workers can conceivably run as long as the the
  // browser is open; we have to cap somewhere.
  constexpr base::TimeDelta kMax = base::TimeDelta::FromDays(1);
  // Set the bucket count to 50 since that is the recommended value for all
  // histograms.
  const int kBucketCount = 50;

  UMA_HISTOGRAM_CUSTOM_TIMES("ServiceWorker.Runtime", time, kMin, kMax,
                             kBucketCount);
}

}  // namespace content
