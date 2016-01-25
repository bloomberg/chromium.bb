// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_METRICS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_METRICS_H_

#include <stddef.h>

#include "base/macros.h"
#include "content/browser/service_worker/service_worker_database.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerResponseError.h"

class GURL;

namespace content {

class ServiceWorkerMetrics {
 public:
  enum ReadResponseResult {
    READ_OK,
    READ_HEADERS_ERROR,
    READ_DATA_ERROR,
    NUM_READ_RESPONSE_RESULT_TYPES,
  };

  enum WriteResponseResult {
    WRITE_OK,
    WRITE_HEADERS_ERROR,
    WRITE_DATA_ERROR,
    NUM_WRITE_RESPONSE_RESULT_TYPES,
  };

  enum DeleteAndStartOverResult {
    DELETE_OK,
    DELETE_DATABASE_ERROR,
    DELETE_DISK_CACHE_ERROR,
    NUM_DELETE_AND_START_OVER_RESULT_TYPES,
  };

  enum URLRequestJobResult {
    REQUEST_JOB_FALLBACK_RESPONSE,
    REQUEST_JOB_FALLBACK_FOR_CORS,
    REQUEST_JOB_HEADERS_ONLY_RESPONSE,
    REQUEST_JOB_STREAM_RESPONSE,
    REQUEST_JOB_BLOB_RESPONSE,
    REQUEST_JOB_ERROR_RESPONSE_STATUS_ZERO,
    REQUEST_JOB_ERROR_BAD_BLOB,
    REQUEST_JOB_ERROR_NO_PROVIDER_HOST,
    REQUEST_JOB_ERROR_NO_ACTIVE_VERSION,
    REQUEST_JOB_ERROR_NO_REQUEST,
    REQUEST_JOB_ERROR_FETCH_EVENT_DISPATCH,
    REQUEST_JOB_ERROR_BLOB_READ,
    REQUEST_JOB_ERROR_STREAM_ABORTED,
    REQUEST_JOB_ERROR_KILLED,
    REQUEST_JOB_ERROR_KILLED_WITH_BLOB,
    REQUEST_JOB_ERROR_KILLED_WITH_STREAM,
    REQUEST_JOB_ERROR_DESTROYED,
    REQUEST_JOB_ERROR_DESTROYED_WITH_BLOB,
    REQUEST_JOB_ERROR_DESTROYED_WITH_STREAM,
    REQUEST_JOB_ERROR_BAD_DELEGATE,
    NUM_REQUEST_JOB_RESULT_TYPES,
  };

  // Used for UMA. Append-only.
  enum class StopStatus {
    NORMAL,
    DETACH_BY_REGISTRY,
    TIMEOUT,
    // Add new types here.
    NUM_TYPES
  };

  // Used for UMA. Append-only.
  enum class EventType {
    ACTIVATE,
    INSTALL,
    FETCH,
    SYNC,
    NOTIFICATION_CLICK,
    PUSH,
    GEOFENCING,
    SERVICE_PORT_CONNECT,
    MESSAGE,
    // Add new events to record here.

    NUM_TYPES
  };

  // Used for UMA. Append only.
  enum class Site { OTHER, NEW_TAB_PAGE, NUM_TYPES };

  // Converts an event type to a string. Used for tracing.
  static const char* EventTypeToString(EventType event_type);

  // Excludes NTP scope from UMA for now as it tends to dominate the stats and
  // makes the results largely skewed. Some metrics don't follow this policy
  // and hence don't call this function.
  static bool ShouldExcludeSiteFromHistogram(Site site);
  static bool ShouldExcludeURLFromHistogram(const GURL& url);

  // Used for ServiceWorkerDiskCache.
  static void CountInitDiskCacheResult(bool result);
  static void CountReadResponseResult(ReadResponseResult result);
  static void CountWriteResponseResult(WriteResponseResult result);

  // Used for ServiceWorkerDatabase.
  static void CountOpenDatabaseResult(ServiceWorkerDatabase::Status status);
  static void CountReadDatabaseResult(ServiceWorkerDatabase::Status status);
  static void CountWriteDatabaseResult(ServiceWorkerDatabase::Status status);
  static void RecordDestroyDatabaseResult(ServiceWorkerDatabase::Status status);

  // Used for ServiceWorkerStorage.
  static void RecordPurgeResourceResult(int net_error);
  static void RecordDeleteAndStartOverResult(DeleteAndStartOverResult result);

  // Counts the number of page loads controlled by a Service Worker.
  static void CountControlledPageLoad(const GURL& url);

  // Records the result of trying to start a worker. |is_installed| indicates
  // whether the version has been installed.
  static void RecordStartWorkerStatus(ServiceWorkerStatusCode status,
                                      bool is_installed);

  // Records the time taken to successfully start a worker. |is_installed|
  // indicates whether the version has been installed.
  static void RecordStartWorkerTime(const base::TimeDelta& time,
                                    bool is_installed);

  // Records the result of trying to stop a worker.
  static void RecordWorkerStopped(StopStatus status);

  // Records the time taken to successfully stop a worker.
  static void RecordStopWorkerTime(const base::TimeDelta& time);

  static void RecordActivateEventStatus(ServiceWorkerStatusCode status);
  static void RecordInstallEventStatus(ServiceWorkerStatusCode status);

  // Records how much of dispatched events are handled while a Service
  // Worker is awake (i.e. after it is woken up until it gets stopped).
  static void RecordEventHandledRatio(EventType event,
                                      size_t handled_events,
                                      size_t fired_events);

  // Records how often a dispatched event times out.
  static void RecordEventTimeout(EventType event);

  // Records the amount of time spent handling an event.
  static void RecordEventDuration(EventType event, const base::TimeDelta& time);

  // Records the result of dispatching a fetch event to a service worker.
  static void RecordFetchEventStatus(bool is_main_resource,
                                     ServiceWorkerStatusCode status);

  // Records the amount of time spent handling a fetch event with the given
  // result.
  static void RecordFetchEventTime(ServiceWorkerFetchEventResult result,
                                   const base::TimeDelta& time);

  // Records result of a ServiceWorkerURLRequestJob that was forwarded to
  // the service worker.
  static void RecordURLRequestJobResult(bool is_main_resource,
                                        URLRequestJobResult result);

  // Records the error code provided when the renderer returns a response with
  // status zero to a fetch request.
  static void RecordStatusZeroResponseError(
      bool is_main_resource,
      blink::WebServiceWorkerResponseError error);

  // Records the mode of request that was fallbacked to the network.
  static void RecordFallbackedRequestMode(FetchRequestMode mode);

  // Called at the beginning of each ServiceWorkerVersion::Dispatch*Event
  // function. Records the time elapsed since idle (generally the time since the
  // previous event ended).
  static void RecordTimeBetweenEvents(const base::TimeDelta& time);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ServiceWorkerMetrics);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_METRICS_H_
