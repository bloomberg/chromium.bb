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
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/common/background_fetch/background_fetch_types.h"
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
  enum class State { INITIALIZED, FETCHING, ABORTED, COMPLETED };

  using CompletedCallback =
      base::OnceCallback<void(BackgroundFetchJobController*)>;

  BackgroundFetchJobController(
      const BackgroundFetchRegistrationId& registration_id,
      const BackgroundFetchOptions& options,
      BrowserContext* browser_context,
      StoragePartition* storage_partition,
      BackgroundFetchDataManager* data_manager,
      CompletedCallback completed_callback);
  ~BackgroundFetchJobController() override;

  // Start processing on a batch of requests. Some of these may already be in
  // progress or completed from a previous chromium instance.
  void StartProcessing();

  // Updates the representation of this Background Fetch in the user interface
  // to match the given |title|.
  void UpdateUI(const std::string& title);

  // Immediately aborts this Background Fetch by request of the developer.
  void Abort();

  // Called by the BackgroundFetchContext when the system is shutting down.
  void Shutdown();

  // Returns the current state of this Job Controller.
  State state() const { return state_; }

  // Returns the registration id for which this job is fetching data.
  const BackgroundFetchRegistrationId& registration_id() const {
    return registration_id_;
  }

  // Returns the options with which this job is fetching data.
  const BackgroundFetchOptions& options() const { return options_; }

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

  // The registration id on behalf of which this controller is fetching data.
  BackgroundFetchRegistrationId registration_id_;

  // Options for the represented background fetch registration.
  BackgroundFetchOptions options_;

  // The current state of this Job Controller.
  State state_ = State::INITIALIZED;

  // TODO(peter): Deprecated, remove in favor of |registration_id|.
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
  CompletedCallback completed_callback_;

  // Map from the GUID assigned by the DownloadManager to the request_guid.
  std::unordered_map<std::string, std::string> download_guid_map_;

  base::WeakPtrFactory<BackgroundFetchJobController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchJobController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_
