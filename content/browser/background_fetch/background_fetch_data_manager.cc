// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include <queue>

#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_constants.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/background_fetch_job_response_data.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"

namespace content {

// The Registration Data class encapsulates the data stored for a particular
// Background Fetch registration. This roughly matches the on-disk format that
// will be adhered to in the future.
class BackgroundFetchDataManager::RegistrationData {
 public:
  RegistrationData(const std::vector<ServiceWorkerFetchRequest>& requests,
                   const BackgroundFetchOptions& options)
      : options_(options) {
    int request_index = 0;

    // Convert the given |requests| to BackgroundFetchRequestInfo objects.
    for (const ServiceWorkerFetchRequest& request : requests)
      requests_.emplace(request_index++, request);
  }

  ~RegistrationData() = default;

  // Returns whether there are remaining requests on the request queue.
  bool HasPendingRequests() const { return !requests_.empty(); }

  // Consumes a request from the queue that is to be fetched.
  BackgroundFetchRequestInfo ConsumeRequest() {
    DCHECK(!requests_.empty());

    BackgroundFetchRequestInfo request = requests_.front();
    requests_.pop();

    return request;
  }

 private:
  std::queue<BackgroundFetchRequestInfo> requests_;
  BackgroundFetchOptions options_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationData);
};

BackgroundFetchDataManager::BackgroundFetchDataManager(
    BrowserContext* browser_context)
    : browser_context_(browser_context), weak_ptr_factory_(this) {
  DCHECK(browser_context_);
}

BackgroundFetchDataManager::~BackgroundFetchDataManager() = default;

void BackgroundFetchDataManager::CreateRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    CreateRegistrationCallback callback) {
  if (registrations_.find(registration_id) != registrations_.end()) {
    std::move(callback).Run(blink::mojom::BackgroundFetchError::DUPLICATED_TAG,
                            std::vector<BackgroundFetchRequestInfo>());
    return;
  }

  std::unique_ptr<RegistrationData> registration_data =
      base::MakeUnique<RegistrationData>(requests, options);

  // Create a vector with the initial requests to feed the Job Controller with.
  std::vector<BackgroundFetchRequestInfo> initial_requests;
  for (size_t i = 0; i < kMaximumBackgroundFetchParallelRequests; ++i) {
    if (!registration_data->HasPendingRequests())
      break;

    initial_requests.push_back(registration_data->ConsumeRequest());
  }

  // Store the created |registration_data| so that we can easily access it.
  registrations_.insert(
      std::make_pair(registration_id, std::move(registration_data)));

  // Inform the |callback| of the newly created registration.
  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE,
                          std::move(initial_requests));
}

void BackgroundFetchDataManager::MarkRequestAsStarted(
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchRequestInfo& request,
    const std::string& download_guid) {
  auto iter = registrations_.find(registration_id);
  DCHECK(iter != registrations_.end());

  // TODO(peter): Associate the |download_guid| with the |request|.
}

void BackgroundFetchDataManager::MarkRequestAsCompleteAndGetNextRequest(
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchRequestInfo& request,
    NextRequestCallback callback) {
  auto iter = registrations_.find(registration_id);
  DCHECK(iter != registrations_.end());

  RegistrationData* registration_data = iter->second.get();

  // TODO(peter): Store the |request| with the |registration_data|.

  base::Optional<BackgroundFetchRequestInfo> next_request;
  if (registration_data->HasPendingRequests())
    next_request = registration_data->ConsumeRequest();

  std::move(callback).Run(next_request);
}

void BackgroundFetchDataManager::DeleteRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    DeleteRegistrationCallback callback) {
  auto iter = registrations_.find(registration_id);
  if (iter == registrations_.end()) {
    std::move(callback).Run(blink::mojom::BackgroundFetchError::INVALID_TAG);
    return;
  }

  registrations_.erase(iter);

  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE);
}

void BackgroundFetchDataManager::WriteJobToStorage(
    std::unique_ptr<BackgroundFetchJobInfo> job_info,
    std::vector<std::unique_ptr<BackgroundFetchRequestInfo>> request_infos) {
  // TODO(harkness): Check for job_guid clash.
  const std::string job_guid = job_info->guid();
  job_map_[job_guid] = std::move(job_info);

  // Make an explicit copy of the original requests
  // TODO(harkness): Replace this with actually writing to storage.
  std::vector<BackgroundFetchRequestInfo> requests;
  for (const auto& request_info : request_infos) {
    requests.emplace_back(*(request_info.get()));
  }
  request_map_[job_guid] = std::move(requests);

  // |request_infos| will be destroyed when it leaves scope here.
}

void BackgroundFetchDataManager::WriteRequestToStorage(
    const std::string& job_guid,
    BackgroundFetchRequestInfo* request_info) {
  std::vector<BackgroundFetchRequestInfo>& request_infos =
      request_map_[job_guid];

  // Copy the updated |request_info| over the in-memory version.
  for (size_t i = 0; i < request_infos.size(); i++) {
    if (request_infos[i].guid() == request_info->guid())
      request_infos[i] = *request_info;
  }
}

std::unique_ptr<BackgroundFetchRequestInfo>
BackgroundFetchDataManager::GetRequestInfo(const std::string& job_guid,
                                           size_t request_index) {
  // Explicitly create a copy. When this is persisted to StorageWorkerStorage,
  // the request_map_ will not exist.
  auto iter = request_map_.find(job_guid);
  DCHECK(iter != request_map_.end());
  const std::vector<BackgroundFetchRequestInfo>& request_infos = iter->second;

  DCHECK(request_index <= request_infos.size());
  return base::MakeUnique<BackgroundFetchRequestInfo>(
      request_infos[request_index]);
}

void BackgroundFetchDataManager::GetJobResponse(
    const std::string& job_guid,
    const BackgroundFetchResponseCompleteCallback& callback) {
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ChromeBlobStorageContext::GetFor, browser_context_),
      base::Bind(&BackgroundFetchDataManager::DidGetBlobStorageContext,
                 weak_ptr_factory_.GetWeakPtr(), job_guid, callback));
}

void BackgroundFetchDataManager::DidGetBlobStorageContext(
    const std::string& job_guid,
    const BackgroundFetchResponseCompleteCallback& callback,
    ChromeBlobStorageContext* blob_context) {
  DCHECK(blob_context);
  BackgroundFetchJobInfo* job_info = job_map_[job_guid].get();
  DCHECK(job_info);

  // Create a BackgroundFetchJobResponseData object which will aggregate
  // together the response blobs.
  job_info->set_job_response_data(
      base::MakeUnique<BackgroundFetchJobResponseData>(job_info->num_requests(),
                                                       callback));
  BackgroundFetchJobResponseData* job_response_data =
      job_info->job_response_data();
  DCHECK(job_response_data);

  // Iterate over the requests and create blobs for each response.
  for (size_t request_index = 0; request_index < job_info->num_requests();
       request_index++) {
    // TODO(harkness): This will need to be asynchronous.
    std::unique_ptr<BackgroundFetchRequestInfo> request_info =
        GetRequestInfo(job_guid, request_index);

    // TODO(harkness): Only create a blob response if the request was
    // successful. Otherwise create an error response.
    std::unique_ptr<BlobHandle> blob_handle =
        blob_context->CreateFileBackedBlob(
            request_info->file_path(), 0 /* offset */,
            request_info->received_bytes(),
            base::Time() /* expected_modification_time */);

    job_response_data->AddResponse(*request_info.get(), std::move(blob_handle));
  }
}

bool BackgroundFetchDataManager::UpdateRequestState(
    const std::string& job_guid,
    const std::string& request_guid,
    DownloadItem::DownloadState state,
    DownloadInterruptReason interrupt_reason) {
  // Find the request and set the state and the interrupt reason.
  BackgroundFetchJobInfo* job_info = job_map_[job_guid].get();
  DCHECK(job_info);
  BackgroundFetchRequestInfo* request =
      job_info->GetActiveRequest(request_guid);
  DCHECK(request);
  request->set_state(state);
  request->set_interrupt_reason(interrupt_reason);

  // If the request is now finished, remove it from the active requests.
  switch (state) {
    case DownloadItem::DownloadState::COMPLETE:
    case DownloadItem::DownloadState::CANCELLED:
      WriteRequestToStorage(job_guid, request);
      job_info->RemoveActiveRequest(request_guid);
      break;
    case DownloadItem::DownloadState::IN_PROGRESS:
    case DownloadItem::DownloadState::INTERRUPTED:
    case DownloadItem::DownloadState::MAX_DOWNLOAD_STATE:
      break;
  }

  // Return a boolean indicating whether there are more requests to be
  // processed.
  return job_info->HasRequestsRemaining();
}

void BackgroundFetchDataManager::UpdateRequestStorageState(
    const std::string& job_guid,
    const std::string& request_guid,
    const base::FilePath& file_path,
    int64_t received_bytes) {
  BackgroundFetchJobInfo* job_info = job_map_[job_guid].get();
  DCHECK(job_info);
  BackgroundFetchRequestInfo* request =
      job_info->GetActiveRequest(request_guid);
  DCHECK(request);
  request->set_file_path(file_path);
  request->set_received_bytes(received_bytes);
}

const BackgroundFetchRequestInfo&
BackgroundFetchDataManager::GetNextBackgroundFetchRequestInfo(
    const std::string& job_guid) {
  BackgroundFetchJobInfo* job_info = job_map_[job_guid].get();
  DCHECK(job_info);

  // TODO(harkness): This needs to be async when it queries real storage.
  std::unique_ptr<BackgroundFetchRequestInfo> request_info =
      GetRequestInfo(job_guid, job_info->next_request_index());
  const std::string request_guid = request_info->guid();
  job_info->AddActiveRequest(std::move(request_info));
  return *job_info->GetActiveRequest(request_guid);
}

bool BackgroundFetchDataManager::IsComplete(const std::string& job_guid) const {
  auto iter = job_map_.find(job_guid);
  DCHECK(iter != job_map_.end());
  return iter->second->IsComplete();
}

bool BackgroundFetchDataManager::HasRequestsRemaining(
    const std::string& job_guid) const {
  auto iter = job_map_.find(job_guid);
  DCHECK(iter != job_map_.end());
  return iter->second->HasRequestsRemaining();
}

void BackgroundFetchDataManager::UpdateRequestDownloadGuid(
    const std::string& job_guid,
    const std::string& request_guid,
    const std::string& download_guid) {
  BackgroundFetchJobInfo* job_info = job_map_[job_guid].get();
  DCHECK(job_info);
  BackgroundFetchRequestInfo* request =
      job_info->GetActiveRequest(request_guid);
  DCHECK(request);
  request->set_download_guid(download_guid);
}

}  // namespace content
