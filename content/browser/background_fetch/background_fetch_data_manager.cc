// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/guid.h"
#include "base/time/time.h"
#include "content/browser/background_fetch/background_fetch_constants.h"
#include "content/browser/background_fetch/background_fetch_cross_origin_filter.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/background_fetch/storage/cleanup_task.h"
#include "content/browser/background_fetch/storage/create_registration_task.h"
#include "content/browser/background_fetch/storage/database_task.h"
#include "content/browser/background_fetch/storage/delete_registration_task.h"
#include "content/browser/background_fetch/storage/get_developer_ids_task.h"
#include "content/browser/background_fetch/storage/get_registration_task.h"
#include "content/browser/background_fetch/storage/mark_registration_for_deletion_task.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/WebKit/common/blob/blob.mojom.h"

namespace content {

namespace {

// Returns whether the response contained in the Background Fetch |request| is
// considered OK. See https://fetch.spec.whatwg.org/#ok-status aka a successful
// 2xx status per https://tools.ietf.org/html/rfc7231#section-6.3.
bool IsOK(const BackgroundFetchRequestInfo& request) {
  int status = request.GetResponseCode();
  return status >= 200 && status < 300;
}

}  // namespace

// The Registration Data class encapsulates the data stored for a particular
// Background Fetch registration. This roughly matches the on-disk format that
// will be adhered to in the future.
class BackgroundFetchDataManager::RegistrationData {
 public:
  RegistrationData(const BackgroundFetchRegistrationId& registration_id,
                   const std::vector<ServiceWorkerFetchRequest>& requests,
                   const BackgroundFetchOptions& options)
      : registration_id_(registration_id), options_(options) {
    int request_index = 0;

    // Convert the given |requests| to BackgroundFetchRequestInfo objects.
    for (const ServiceWorkerFetchRequest& fetch_request : requests) {
      pending_requests_.push(base::MakeRefCounted<BackgroundFetchRequestInfo>(
          request_index++, fetch_request));
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

    request->InitializeDownloadGuid();

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

    complete_requests_downloaded_bytes_ += request->GetFileSize();

    bool has_pending_or_active_requests =
        !pending_requests_.empty() || !active_requests_.empty();
    return has_pending_or_active_requests;
  }

  // Returns the vector with all completed requests part of this registration.
  const std::vector<scoped_refptr<BackgroundFetchRequestInfo>>&
  GetCompletedRequests() const {
    return completed_requests_;
  }

  void SetTitle(const std::string& title) { options_.title = title; }

  const BackgroundFetchRegistrationId& registration_id() const {
    return registration_id_;
  }

  const BackgroundFetchOptions& options() const { return options_; }

  uint64_t GetDownloaded() const { return complete_requests_downloaded_bytes_; }

  int GetTotalNumberOfRequests() const {
    return pending_requests_.size() + active_requests_.size() +
           completed_requests_.size();
  }

 private:
  BackgroundFetchRegistrationId registration_id_;
  BackgroundFetchOptions options_;
  // Number of bytes downloaded as part of completed downloads. (In-progress
  // downloads are tracked elsewhere).
  uint64_t complete_requests_downloaded_bytes_ = 0;

  base::queue<scoped_refptr<BackgroundFetchRequestInfo>> pending_requests_;
  std::vector<scoped_refptr<BackgroundFetchRequestInfo>> active_requests_;

  // TODO(peter): Right now it's safe for this to be a vector because we only
  // allow a single parallel request. That stops when we start allowing more.
  static_assert(kMaximumBackgroundFetchParallelRequests == 1,
                "RegistrationData::completed_requests_ assumes no parallelism");

  std::vector<scoped_refptr<BackgroundFetchRequestInfo>> completed_requests_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationData);
};

BackgroundFetchDataManager::BackgroundFetchDataManager(
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : service_worker_context_(std::move(service_worker_context)),
      weak_ptr_factory_(this) {
  // Constructed on the UI thread, then used on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);

  // Store the blob storage context for the given |browser_context|.
  blob_storage_context_ =
      base::WrapRefCounted(ChromeBlobStorageContext::GetFor(browser_context));
  DCHECK(blob_storage_context_);

  BrowserThread::PostAfterStartupTask(
      FROM_HERE, BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
      // Normally weak pointers must be obtained on the IO thread, but it's ok
      // here as the factory cannot be destroyed before the constructor ends.
      base::Bind(&BackgroundFetchDataManager::Cleanup,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundFetchDataManager::Cleanup() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBackgroundFetchPersistence)) {
    AddDatabaseTask(std::make_unique<background_fetch::CleanupTask>(this));
  }
}

BackgroundFetchDataManager::~BackgroundFetchDataManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundFetchDataManager::CreateRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBackgroundFetchPersistence)) {
    AddDatabaseTask(std::make_unique<background_fetch::CreateRegistrationTask>(
        this, registration_id, requests, options, std::move(callback)));
    return;
  }

  // New registrations should never re-use a |unique_id|.
  DCHECK_EQ(0u, registrations_.count(registration_id.unique_id()));

  auto developer_id_tuple =
      std::make_tuple(registration_id.service_worker_registration_id(),
                      registration_id.origin(), registration_id.developer_id());

  if (active_registration_unique_ids_.count(developer_id_tuple)) {
    std::move(callback).Run(
        blink::mojom::BackgroundFetchError::DUPLICATED_DEVELOPER_ID, nullptr);
    return;
  }

  // Mark |unique_id| as the currently active registration for
  // |developer_id_tuple|.
  active_registration_unique_ids_.emplace(std::move(developer_id_tuple),
                                          registration_id.unique_id());

  // Create the |RegistrationData|, and store it for easy access.
  registrations_.emplace(
      registration_id.unique_id(),
      std::make_unique<RegistrationData>(registration_id, requests, options));

  // Re-use GetRegistration to compile the BackgroundFetchRegistration object.
  // WARNING: GetRegistration doesn't use the |unique_id| when looking up the
  // registration data. That's fine here, since we are calling it synchronously
  // immediately after writing the |unique_id| to |active_unique_ids_|. But if
  // GetRegistation becomes async, it will no longer be safe to do this.
  GetRegistration(registration_id.service_worker_registration_id(),
                  registration_id.origin(), registration_id.developer_id(),
                  std::move(callback));
}

void BackgroundFetchDataManager::GetRegistration(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& developer_id,
    GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBackgroundFetchPersistence)) {
    AddDatabaseTask(std::make_unique<background_fetch::GetRegistrationTask>(
        this, service_worker_registration_id, origin, developer_id,
        std::move(callback)));
    return;
  }

  auto developer_id_tuple =
      std::make_tuple(service_worker_registration_id, origin, developer_id);

  auto iter = active_registration_unique_ids_.find(developer_id_tuple);
  if (iter == active_registration_unique_ids_.end()) {
    std::move(callback).Run(blink::mojom::BackgroundFetchError::INVALID_ID,
                            nullptr /* registration */);
    return;
  }

  const std::string& unique_id = iter->second;

  DCHECK_EQ(1u, registrations_.count(unique_id));
  RegistrationData* data = registrations_[unique_id].get();

  // Compile the BackgroundFetchRegistration object for the developer.
  auto registration = std::make_unique<BackgroundFetchRegistration>();
  registration->developer_id = developer_id;
  registration->unique_id = unique_id;
  registration->title = data->options().title;
  // TODO(crbug.com/774054): Uploads are not yet supported.
  registration->upload_total = 0;
  registration->uploaded = 0;
  registration->download_total = data->options().download_total;
  registration->downloaded = data->GetDownloaded();

  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE,
                          std::move(registration));
}

void BackgroundFetchDataManager::UpdateRegistrationUI(
    const std::string& unique_id,
    const std::string& title,
    blink::mojom::BackgroundFetchService::UpdateUICallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto registrations_iter = registrations_.find(unique_id);
  if (registrations_iter == registrations_.end()) {  // Not found.
    std::move(callback).Run(blink::mojom::BackgroundFetchError::INVALID_ID);
    return;
  }

  // Update stored registration.
  registrations_iter->second->SetTitle(title);

  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE);
}

void BackgroundFetchDataManager::PopNextRequest(
    const BackgroundFetchRegistrationId& registration_id,
    NextRequestCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!IsActive(registration_id)) {
    // Stop giving out requests as registration aborted (or otherwise finished).
    std::move(callback).Run(nullptr /* request */);
    return;
  }

  auto registration_iter = registrations_.find(registration_id.unique_id());
  DCHECK(registration_iter != registrations_.end());

  RegistrationData* registration_data = registration_iter->second.get();

  scoped_refptr<BackgroundFetchRequestInfo> next_request;
  if (registration_data->HasPendingRequests())
    next_request = registration_data->PopNextPendingRequest();

  std::move(callback).Run(std::move(next_request));
}

void BackgroundFetchDataManager::MarkRequestAsStarted(
    const BackgroundFetchRegistrationId& registration_id,
    BackgroundFetchRequestInfo* request,
    const std::string& download_guid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = registrations_.find(registration_id.unique_id());
  DCHECK(iter != registrations_.end());

  RegistrationData* registration_data = iter->second.get();
  registration_data->MarkRequestAsStarted(request, download_guid);
}

void BackgroundFetchDataManager::MarkRequestAsComplete(
    const BackgroundFetchRegistrationId& registration_id,
    BackgroundFetchRequestInfo* request,
    MarkedCompleteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = registrations_.find(registration_id.unique_id());
  DCHECK(iter != registrations_.end());

  RegistrationData* registration_data = iter->second.get();
  bool has_pending_or_active_requests =
      registration_data->MarkRequestAsComplete(request);

  std::move(callback).Run(has_pending_or_active_requests);
}

void BackgroundFetchDataManager::GetSettledFetchesForRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    SettledFetchesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = registrations_.find(registration_id.unique_id());
  DCHECK(iter != registrations_.end());

  RegistrationData* registration_data = iter->second.get();
  DCHECK(!registration_data->HasPendingRequests());

  const std::vector<scoped_refptr<BackgroundFetchRequestInfo>>& requests =
      registration_data->GetCompletedRequests();

  bool background_fetch_succeeded = true;

  std::vector<BackgroundFetchSettledFetch> settled_fetches;
  settled_fetches.reserve(requests.size());

  std::vector<std::unique_ptr<storage::BlobDataHandle>> blob_data_handles;

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
        DCHECK(blob_storage_context_);

        storage::BlobDataBuilder blob_builder(base::GenerateGUID());
        blob_builder.AppendFile(request->GetFilePath(), 0 /* offset */,
                                request->GetFileSize(),
                                base::Time() /* expected_modification_time */);

        auto blob_data_handle =
            GetBlobStorageContext(blob_storage_context_.get())
                ->AddFinishedBlob(&blob_builder);

        // TODO(peter): Appropriately handle !blob_data_handle
        if (blob_data_handle) {
          settled_fetch.response.blob_uuid = blob_data_handle->uuid();
          settled_fetch.response.blob_size = blob_data_handle->size();
          if (features::IsMojoBlobsEnabled()) {
            blink::mojom::BlobPtr blob_ptr;
            storage::BlobImpl::Create(
                std::make_unique<storage::BlobDataHandle>(*blob_data_handle),
                MakeRequest(&blob_ptr));

            settled_fetch.response.blob =
                base::MakeRefCounted<storage::BlobHandle>(std::move(blob_ptr));
          }

          blob_data_handles.push_back(std::move(blob_data_handle));
        }
      }
    } else {
      // TODO(crbug.com/711354): Consider Background Fetches as failed when the
      // response cannot be relayed to the developer.
      background_fetch_succeeded = false;
    }

    // TODO(delphick): settled_fetch.response.error
    settled_fetch.response.response_time = request->GetResponseTime();
    // TODO(delphick): settled_fetch.response.cors_exposed_header_names

    background_fetch_succeeded = background_fetch_succeeded && IsOK(*request);

    settled_fetches.push_back(settled_fetch);
  }

  std::move(callback).Run(
      blink::mojom::BackgroundFetchError::NONE, background_fetch_succeeded,
      std::move(settled_fetches), std::move(blob_data_handles));
}

void BackgroundFetchDataManager::MarkRegistrationForDeletion(
    const BackgroundFetchRegistrationId& registration_id,
    HandleBackgroundFetchErrorCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBackgroundFetchPersistence)) {
    AddDatabaseTask(
        std::make_unique<background_fetch::MarkRegistrationForDeletionTask>(
            this, registration_id, std::move(callback)));
    return;
  }

  auto developer_id_tuple =
      std::make_tuple(registration_id.service_worker_registration_id(),
                      registration_id.origin(), registration_id.developer_id());

  auto active_unique_id_iter =
      active_registration_unique_ids_.find(developer_id_tuple);

  // The |unique_id| must also match, as a website can create multiple
  // registrations with the same |developer_id_tuple| (even though only one can
  // be active at once).
  if (active_unique_id_iter == active_registration_unique_ids_.end() ||
      active_unique_id_iter->second != registration_id.unique_id()) {
    std::move(callback).Run(blink::mojom::BackgroundFetchError::INVALID_ID);
    return;
  }

  active_registration_unique_ids_.erase(active_unique_id_iter);

  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE);
}

void BackgroundFetchDataManager::DeleteRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    HandleBackgroundFetchErrorCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBackgroundFetchPersistence)) {
    AddDatabaseTask(std::make_unique<background_fetch::DeleteRegistrationTask>(
        this, registration_id.service_worker_registration_id(),
        registration_id.unique_id(), std::move(callback)));
    return;
  }

  DCHECK(!IsActive(registration_id))
      << "MarkRegistrationForDeletion must already have been called";

  std::move(callback).Run(registrations_.erase(registration_id.unique_id())
                              ? blink::mojom::BackgroundFetchError::NONE
                              : blink::mojom::BackgroundFetchError::INVALID_ID);
}

void BackgroundFetchDataManager::GetDeveloperIdsForServiceWorker(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    blink::mojom::BackgroundFetchService::GetDeveloperIdsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBackgroundFetchPersistence)) {
    AddDatabaseTask(std::make_unique<background_fetch::GetDeveloperIdsTask>(
        this, service_worker_registration_id, origin, std::move(callback)));
    return;
  }

  std::vector<std::string> developer_ids;
  for (const auto& entry : active_registration_unique_ids_) {
    if (service_worker_registration_id == std::get<0>(entry.first) &&
        origin == std::get<1>(entry.first)) {
      developer_ids.emplace_back(std::get<2>(entry.first));
    }
  }

  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE,
                          developer_ids);
}

int BackgroundFetchDataManager::GetTotalNumberOfRequests(
    const BackgroundFetchRegistrationId& registration_id) const {
  return registrations_.find(registration_id.unique_id())
      ->second->GetTotalNumberOfRequests();
}

bool BackgroundFetchDataManager::IsActive(
    const BackgroundFetchRegistrationId& registration_id) {
  auto developer_id_tuple =
      std::make_tuple(registration_id.service_worker_registration_id(),
                      registration_id.origin(), registration_id.developer_id());

  auto active_unique_id_iter =
      active_registration_unique_ids_.find(developer_id_tuple);

  // The |unique_id| must also match, as a website can create multiple
  // registrations with the same |developer_id_tuple| (even though only one can
  // be active at once).
  return active_unique_id_iter != active_registration_unique_ids_.end() &&
         active_unique_id_iter->second == registration_id.unique_id();
}

void BackgroundFetchDataManager::AddDatabaseTask(
    std::unique_ptr<background_fetch::DatabaseTask> task) {
  database_tasks_.push(std::move(task));
  if (database_tasks_.size() == 1)
    database_tasks_.front()->Start();
}

void BackgroundFetchDataManager::OnDatabaseTaskFinished(
    background_fetch::DatabaseTask* task) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(!database_tasks_.empty());
  DCHECK_EQ(database_tasks_.front().get(), task);

  database_tasks_.pop();
  if (!database_tasks_.empty())
    database_tasks_.front()->Start();
}

}  // namespace content
