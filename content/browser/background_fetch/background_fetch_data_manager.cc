// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include <algorithm>
#include <queue>

#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_constants.h"
#include "content/browser/background_fetch/background_fetch_context.h"
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
      pending_requests_.emplace(request_index++, request);
  }

  ~RegistrationData() = default;

  // Returns whether there are remaining requests on the request queue.
  bool HasPendingRequests() const { return !pending_requests_.empty(); }

  // Consumes a request from the queue that is to be fetched.
  BackgroundFetchRequestInfo GetPendingRequest() {
    DCHECK(!pending_requests_.empty());

    BackgroundFetchRequestInfo request = pending_requests_.front();
    pending_requests_.pop();

    // The |request| is considered to be active now.
    active_requests_.push_back(request);

    return request;
  }

  // Marks the |request| as having started with the given |download_guid|.
  // Persistent storage needs to store the association so we can resume fetches
  // after a browser restart, here we just verify that the |request| is active.
  void MarkRequestAsStarted(const BackgroundFetchRequestInfo& request,
                            const std::string& download_guid) {
    const auto iter = std::find_if(
        active_requests_.begin(), active_requests_.end(),
        [&request](const BackgroundFetchRequestInfo& active_request) {
          return active_request.request_index() == request.request_index();
        });

    // The |request| must have been consumed from this RegistrationData.
    DCHECK(iter != active_requests_.end());
  }

  // Marks the |request| as having completed. Verifies that the |request| is
  // currently active and moves it to the |completed_requests_| vector.
  void MarkRequestAsComplete(const BackgroundFetchRequestInfo& request) {
    const auto iter = std::find_if(
        active_requests_.begin(), active_requests_.end(),
        [&request](const BackgroundFetchRequestInfo& active_request) {
          return active_request.request_index() == request.request_index();
        });

    // The |request| must have been consumed from this RegistrationData.
    DCHECK(iter != active_requests_.end());

    active_requests_.erase(iter);
    completed_requests_.push_back(request);
  }

  // Returns the vector with all completed requests part of this registration.
  const std::vector<BackgroundFetchRequestInfo>& GetCompletedRequests() const {
    return completed_requests_;
  }

 private:
  BackgroundFetchOptions options_;

  // TODO(peter): BackgroundFetchRequestInfo should be stored in a
  // unique_ptr, owned by the BackgroundFetchJobController. Right now we
  // can't do that as we need to hold on to copies here.
  std::queue<BackgroundFetchRequestInfo> pending_requests_;
  std::vector<BackgroundFetchRequestInfo> active_requests_;

  // TODO(peter): Right now it's safe for this to be a vector because we only
  // allow a single parallel request. That stops when we start allowing more.
  static_assert(kMaximumBackgroundFetchParallelRequests == 1,
                "RegistrationData::completed_requests_ assumes no parallelism");

  std::vector<BackgroundFetchRequestInfo> completed_requests_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationData);
};

BackgroundFetchDataManager::BackgroundFetchDataManager(
    BrowserContext* browser_context)
    : weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);

  // Store the blob storage context for the given |browser_context|.
  blob_storage_context_ =
      make_scoped_refptr(ChromeBlobStorageContext::GetFor(browser_context));
  DCHECK(blob_storage_context_);
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

    initial_requests.push_back(registration_data->GetPendingRequest());
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

  RegistrationData* registration_data = iter->second.get();
  registration_data->MarkRequestAsStarted(request, download_guid);
}

void BackgroundFetchDataManager::MarkRequestAsCompleteAndGetNextRequest(
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchRequestInfo& request,
    NextRequestCallback callback) {
  auto iter = registrations_.find(registration_id);
  DCHECK(iter != registrations_.end());

  RegistrationData* registration_data = iter->second.get();
  registration_data->MarkRequestAsComplete(request);

  base::Optional<BackgroundFetchRequestInfo> next_request;
  if (registration_data->HasPendingRequests())
    next_request = registration_data->GetPendingRequest();

  std::move(callback).Run(next_request);
}

void BackgroundFetchDataManager::GetSettledFetchesForRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    SettledFetchesCallback callback) {
  auto iter = registrations_.find(registration_id);
  DCHECK(iter != registrations_.end());

  RegistrationData* registration_data = iter->second.get();
  DCHECK(!registration_data->HasPendingRequests());

  const std::vector<BackgroundFetchRequestInfo>& requests =
      registration_data->GetCompletedRequests();

  std::vector<BackgroundFetchSettledFetch> settled_fetches;
  settled_fetches.reserve(requests.size());

  std::vector<std::unique_ptr<BlobHandle>> blob_handles;

  for (const auto& request : requests) {
    BackgroundFetchSettledFetch settled_fetch;
    settled_fetch.request = request.fetch_request();

    settled_fetch.response.url_list.push_back(request.GetURL());
    // TODO: settled_fetch.response.status_code
    // TODO: settled_fetch.response.status_text
    // TODO: settled_fetch.response.response_type
    // TODO: settled_fetch.response.headers

    if (request.received_bytes() > 0) {
      DCHECK(!request.file_path().empty());

      std::unique_ptr<BlobHandle> blob_handle =
          blob_storage_context_->CreateFileBackedBlob(
              request.file_path(), 0 /* offset */, request.received_bytes(),
              base::Time() /* expected_modification_time */);

      // TODO(peter): Appropriately handle !blob_handle
      if (blob_handle) {
        settled_fetch.response.blob_uuid = blob_handle->GetUUID();
        settled_fetch.response.blob_size = request.received_bytes();

        blob_handles.push_back(std::move(blob_handle));
      }
    }

    // TODO: settled_fetch.response.error
    // TODO: settled_fetch.response.response_time
    // TODO: settled_fetch.response.cors_exposed_header_names

    settled_fetches.push_back(settled_fetch);
  }

  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE,
                          std::move(settled_fetches), std::move(blob_handles));
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

}  // namespace content
