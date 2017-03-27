// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_item.h"

namespace content {

class BackgroundFetchDataManager;
class BackgroundFetchRequestInfo;
class BrowserContext;
class StoragePartition;

// The JobController will be responsible for coordinating communication with the
// DownloadManager. It will get requests from the DataManager and dispatch them
// to the DownloadManager. It lives entirely on the IO thread.
class CONTENT_EXPORT BackgroundFetchJobController
    : public DownloadItem::Observer {
 public:
  BackgroundFetchJobController(const std::string& job_guid,
                               BrowserContext* browser_context,
                               StoragePartition* storage_partition,
                               BackgroundFetchDataManager* data_manager,
                               base::OnceClosure completed_closure);
  ~BackgroundFetchJobController() override;

  // Start processing on a batch of requests. Some of these may already be in
  // progress or completed from a previous chromium instance.
  void StartProcessing();

  // Called by the BackgroundFetchContext when the system is shutting down.
  void Shutdown();

 private:
  // DownloadItem::Observer methods.
  void OnDownloadUpdated(DownloadItem* item) override;
  void OnDownloadDestroyed(DownloadItem* item) override;

  // Callback passed to the DownloadManager which will be invoked once the
  // download starts.
  void DownloadStarted(const std::string& request_guid,
                       DownloadItem* item,
                       DownloadInterruptReason reason);

  void ProcessRequest(const BackgroundFetchRequestInfo& request);

  std::string job_guid_;

  // Pointer to the browser context. The BackgroundFetchJobController is owned
  // by the BrowserContext via the StoragePartition.
  // TODO(harkness): Currently this is only used to lookup the DownloadManager.
  // Investigate whether the DownloadManager should be passed instead.
  BrowserContext* browser_context_;

  // Pointer to the storage partition. This object is owned by the partition
  // (through a sequence of other classes).
  StoragePartition* storage_partition_;

  // The DataManager's lifetime is controlled by the BackgroundFetchContext and
  // will be kept alive until after the JobController is destroyed.
  BackgroundFetchDataManager* data_manager_;

  // Callback for when all fetches have been completed.
  base::OnceClosure completed_closure_;

  // Map from the GUID assigned by the DownloadManager to the request_guid.
  std::unordered_map<std::string, std::string> download_guid_map_;

  base::WeakPtrFactory<BackgroundFetchJobController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchJobController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_
