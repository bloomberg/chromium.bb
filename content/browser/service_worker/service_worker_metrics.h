// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_METRICS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_METRICS_H_

#include "base/macros.h"
#include "content/browser/service_worker/service_worker_database.h"

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

  static void RecordActivateEventStatus(ServiceWorkerStatusCode status);
  static void RecordInstallEventStatus(ServiceWorkerStatusCode status);

  // Records the ratio of unhandled events to the all events fired during
  // the lifetime of ServiceWorker.
  static void RecordEventStatus(size_t fired_events, size_t handled_events);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ServiceWorkerMetrics);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_METRICS_H_
