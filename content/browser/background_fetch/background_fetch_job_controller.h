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
#include "base/optional.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_item.h"

namespace content {

class BackgroundFetchDataManager;
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

  // Starts fetching the |initial_fetches|. The controller will continue to
  // fetch new content until all requests have been handled.
  void Start(std::vector<BackgroundFetchRequestInfo> initial_requests);

  // Updates the representation of this Background Fetch in the user interface
  // to match the given |title|.
  void UpdateUI(const std::string& title);

  // Immediately aborts this Background Fetch by request of the developer.
  void Abort();

  // Returns the current state of this Job Controller.
  State state() const { return state_; }

  // Returns the registration id for which this job is fetching data.
  const BackgroundFetchRegistrationId& registration_id() const {
    return registration_id_;
  }

  // Returns the options with which this job is fetching data.
  const BackgroundFetchOptions& options() const { return options_; }

  // DownloadItem::Observer methods.
  void OnDownloadUpdated(DownloadItem* item) override;
  void OnDownloadDestroyed(DownloadItem* item) override;

 private:
  // Requests the download manager to start fetching |request|.
  void StartRequest(const BackgroundFetchRequestInfo& request);

  // Called when the request identified by |request_index| has been started.
  // The |download_item| continues to be owned by the download system. The
  // |interrupt_reason| will indicate when a request could not be started.
  void DidStartRequest(const BackgroundFetchRequestInfo& request,
                       DownloadItem* download_item,
                       DownloadInterruptReason interrupt_reason);

  // Called when a completed download has been marked as such in the DataManager
  // and the next request, if any, has been read from storage.
  void DidGetNextRequest(
      const base::Optional<BackgroundFetchRequestInfo>& request);

  // The registration id on behalf of which this controller is fetching data.
  BackgroundFetchRegistrationId registration_id_;

  // Options for the represented background fetch registration.
  BackgroundFetchOptions options_;

  // The current state of this Job Controller.
  State state_ = State::INITIALIZED;

  // Map from DownloadItem* to the request info for the in-progress downloads.
  std::unordered_map<DownloadItem*, BackgroundFetchRequestInfo> downloads_;

  // Number of outstanding acknowledgements we expect to get from the
  // DataManager about
  int pending_completed_file_acknowledgements_ = 0;

  // The BrowserContext that indirectly owns us.
  BrowserContext* browser_context_;

  // Pointer to the storage partition. This object is owned by the partition
  // (through a sequence of other classes).
  StoragePartition* storage_partition_;

  // The DataManager's lifetime is controlled by the BackgroundFetchContext and
  // will be kept alive until after the JobController is destroyed.
  BackgroundFetchDataManager* data_manager_;

  // Callback for when all fetches have been completed.
  CompletedCallback completed_callback_;

  base::WeakPtrFactory<BackgroundFetchJobController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchJobController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_JOB_CONTROLLER_H_
