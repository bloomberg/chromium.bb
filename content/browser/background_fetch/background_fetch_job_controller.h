// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/common/content_export.h"

namespace content {

class BrowserContext;
class StoragePartition;

// The JobController will be responsible for coordinating communication with the
// DownloadManager. It will get requests from the JobData and dispatch them to
// the DownloadManager.
// TODO(harkness): The JobController should also observe downloads.
class CONTENT_EXPORT BackgroundFetchJobController {
 public:
  BackgroundFetchJobController(BrowserContext* browser_context,
                               StoragePartition* storage_partition);
  ~BackgroundFetchJobController();

  // Start processing on a batch of requests. Some of these may already be in
  // progress or completed from a previous chromium instance.
  void ProcessJob(const std::string& job_guid,
                  BackgroundFetchJobData* job_data);

 private:
  void ProcessRequest(const std::string& job_guid,
                      const BackgroundFetchRequestInfo& request);

  // Pointer to the browser context. The BackgroundFetchJobController is owned
  // by the BrowserContext via the StoragePartition.
  BrowserContext* browser_context_;

  // Pointer to the storage partition. This object is owned by the partition
  // (through a sequence of other classes).
  StoragePartition* storage_partition_;

  // The fetch_map holds any requests which have been sent to the
  // DownloadManager indexed by the job_guid.
  std::unordered_map<std::string, BackgroundFetchRequestInfo> fetch_map_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchJobController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_
