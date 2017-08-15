// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include <algorithm>
#include <queue>

#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_constants.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/background_fetch_cross_origin_filter.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "services/network/public/interfaces/fetch_api.mojom.h"

namespace content {

// Returns whether the response contained in the Background Fetch |request| is
// considered OK. See https://fetch.spec.whatwg.org/#ok-status aka a successful
// 2xx status per https://tools.ietf.org/html/rfc7231#section-6.3.
bool IsOK(const BackgroundFetchRequestInfo& request) {
  int status = request.GetResponseCode();
  return status >= 200 && status < 300;
}

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
    for (const ServiceWorkerFetchRequest& fetch_request : requests) {
      scoped_refptr<BackgroundFetchRequestInfo> request =
          new BackgroundFetchRequestInfo(request_index++, fetch_request);

      pending_requests_.push(std::move(request));
    }
  }

  ~RegistrationData() = default;

  // Returns whether there are remaining requests on the request queue.
  bool HasPendingRequests() const { return !pending_requests_.empty(); }

  // Consumes a request from the queue that is to be fetched.
  scoped_refptr<BackgroundFetchRequestInfo> PopNextPendingRequest() {
    DCHECK(!pending_requests_.empty());

    auto request = pending_requests_.front();
    pending_requests_.pop();

    // The |request| is considered to be active now.
    active_requests_.push_back(request);

    return request;
  }

  // Marks the |request| as having started with the given |download_guid|.
  // Persistent storage needs to store the association so we can resume fetches
  // after a browser restart, here we just verify that the |request| is active.
  void MarkRequestAsStarted(BackgroundFetchRequestInfo* request,
                            const std::string& download_guid) {
    const auto iter = std::find_if(
        active_requests_.begin(), active_requests_.end(),
        [&request](scoped_refptr<BackgroundFetchRequestInfo> active_request) {
          return active_request->request_index() == request->request_index();
        });

    // The |request| must have been consumed from this RegistrationData.
    DCHECK(iter != active_requests_.end());
  }

  // Marks the |request| as having completed. Verifies that the |request| is
  // currently active and moves it to the |completed_requests_| vector.
  bool MarkRequestAsComplete(BackgroundFetchRequestInfo* request) {
    const auto iter = std::find_if(
        active_requests_.begin(), active_requests_.end(),
        [&request](scoped_refptr<BackgroundFetchRequestInfo> active_request) {
          return active_request->request_index() == request->request_index();
        });

    // The |request| must have been consumed from this RegistrationData.
    DCHECK(iter != active_requests_.end());

    completed_requests_.push_back(*iter);
    active_requests_.erase(iter);

    bool has_pending_or_active_requests =
        !pending_requests_.empty() || !active_requests_.empty();
    return has_pending_or_active_requests;
  }

  // Returns the vector with all completed requests part of this registration.
  const std::vector<scoped_refptr<BackgroundFetchRequestInfo>>&
  GetCompletedRequests() const {
    return completed_requests_;
  }

 private:
  BackgroundFetchOptions options_;

  std::queue<scoped_refptr<BackgroundFetchRequestInfo>> pending_requests_;
  std::vector<scoped_refptr<BackgroundFetchRequestInfo>> active_requests_;

  // TODO(peter): Right now it's safe for this to be a vector because we only
  // allow a single parallel request. That stops when we start allowing more.
  static_assert(kMaximumBackgroundFetchParallelRequests == 1,
                "RegistrationData::completed_requests_ assumes no parallelism");

  std::vector<scoped_refptr<BackgroundFetchRequestInfo>> completed_requests_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationData);
};

BackgroundFetchDataManager::BackgroundFetchDataManager(
    BrowserContext* browser_context)
    : weak_ptr_factory_(this) {
  // Constructed on the UI thread, then used on a different thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DETACH_FROM_SEQUENCE(sequence_checker_);

  DCHECK(browser_context);

  // Store the blob storage context for the given |browser_context|.
  blob_storage_context_ =
      make_scoped_refptr(ChromeBlobStorageContext::GetFor(browser_context));
  DCHECK(blob_storage_context_);
}

BackgroundFetchDataManager::~BackgroundFetchDataManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BackgroundFetchDataManager::CreateRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    CreateRegistrationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (registrations_.find(registration_id) != registrations_.end()) {
    std::move(callback).Run(blink::mojom::BackgroundFetchError::DUPLICATED_ID);
    return;
  }

  // Create the |RegistrationData|, and store it for easy access.
  registrations_.insert(std::make_pair(
      registration_id, base::MakeUnique<RegistrationData>(requests, options)));

  // Inform the |callback| of the newly created registration.
  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE);
}

void BackgroundFetchDataManager::PopNextRequest(
    const BackgroundFetchRegistrationId& registration_id,
    NextRequestCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = registrations_.find(registration_id);
  DCHECK(iter != registrations_.end());

  RegistrationData* registration_data = iter->second.get();

  scoped_refptr<BackgroundFetchRequestInfo> next_request;
  if (registration_data->HasPendingRequests())
    next_request = registration_data->PopNextPendingRequest();

  std::move(callback).Run(std::move(next_request));
}

void BackgroundFetchDataManager::MarkRequestAsStarted(
    const BackgroundFetchRegistrationId& registration_id,
    BackgroundFetchRequestInfo* request,
    const std::string& download_guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = registrations_.find(registration_id);
  DCHECK(iter != registrations_.end());

  RegistrationData* registration_data = iter->second.get();
  registration_data->MarkRequestAsStarted(request, download_guid);
}

void BackgroundFetchDataManager::MarkRequestAsComplete(
    const BackgroundFetchRegistrationId& registration_id,
    BackgroundFetchRequestInfo* request,
    MarkedCompleteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = registrations_.find(registration_id);
  DCHECK(iter != registrations_.end());

  RegistrationData* registration_data = iter->second.get();
  bool has_pending_or_active_requests =
      registration_data->MarkRequestAsComplete(request);

  std::move(callback).Run(has_pending_or_active_requests);
}

void BackgroundFetchDataManager::GetSettledFetchesForRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    SettledFetchesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = registrations_.find(registration_id);
  DCHECK(iter != registrations_.end());

  RegistrationData* registration_data = iter->second.get();
  DCHECK(!registration_data->HasPendingRequests());

  const std::vector<scoped_refptr<BackgroundFetchRequestInfo>>& requests =
      registration_data->GetCompletedRequests();

  bool background_fetch_succeeded = true;

  std::vector<BackgroundFetchSettledFetch> settled_fetches;
  settled_fetches.reserve(requests.size());

  std::vector<std::unique_ptr<BlobHandle>> blob_handles;

  for (const auto& request : requests) {
    BackgroundFetchSettledFetch settled_fetch;
    settled_fetch.request = request->fetch_request();

    // The |filter| decides which values can be passed on to the Service Worker.
    BackgroundFetchCrossOriginFilter filter(registration_id.origin(), *request);

    settled_fetch.response.url_list = request->GetURLChain();
    settled_fetch.response.response_type =
        network::mojom::FetchResponseType::kDefault;

    // Include the status code, status text and the response's body as a blob
    // when this is allowed by the CORS protocol.
    if (filter.CanPopulateBody()) {
      settled_fetch.response.status_code = request->GetResponseCode();
      settled_fetch.response.status_text = request->GetResponseText();
      settled_fetch.response.headers.insert(
          request->GetResponseHeaders().begin(),
          request->GetResponseHeaders().end());

      if (request->GetFileSize() > 0) {
        DCHECK(!request->GetFilePath().empty());
        // CreateFileBackedBlob DCHECKs that it is called on the IO thread. This
        // imposes a more specific requirement than our sequence_checker_.
        std::unique_ptr<BlobHandle> blob_handle =
            blob_storage_context_->CreateFileBackedBlob(
                request->GetFilePath(), 0 /* offset */, request->GetFileSize(),
                base::Time() /* expected_modification_time */);

        // TODO(peter): Appropriately handle !blob_handle
        if (blob_handle) {
          settled_fetch.response.blob_uuid = blob_handle->GetUUID();
          settled_fetch.response.blob_size = request->GetFileSize();

          blob_handles.push_back(std::move(blob_handle));
        }
      }
    } else {
      // TODO(crbug.com/711354): Consider Background Fetches as failed when the
      // response cannot be relayed to the developer.
      background_fetch_succeeded = false;
    }

    // TODO: settled_fetch.response.error
    settled_fetch.response.response_time = request->GetResponseTime();
    // TODO: settled_fetch.response.cors_exposed_header_names

    background_fetch_succeeded = background_fetch_succeeded && IsOK(*request);

    settled_fetches.push_back(settled_fetch);
  }

  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE,
                          background_fetch_succeeded,
                          std::move(settled_fetches), std::move(blob_handles));
}

void BackgroundFetchDataManager::DeleteRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    DeleteRegistrationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = registrations_.find(registration_id);
  if (iter == registrations_.end()) {
    std::move(callback).Run(blink::mojom::BackgroundFetchError::INVALID_ID);
    return;
  }

  registrations_.erase(iter);

  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE);
}

}  // namespace content
