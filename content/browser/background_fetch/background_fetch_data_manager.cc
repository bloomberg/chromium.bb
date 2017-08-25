// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include <algorithm>
#include <queue>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_constants.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/background_fetch_cross_origin_filter.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/interfaces/fetch_api.mojom.h"

namespace content {

namespace {

enum class DatabaseStatus { kOk, kFailed, kNotFound };

DatabaseStatus ToDatabaseStatus(ServiceWorkerStatusCode status) {
  switch (status) {
    case SERVICE_WORKER_OK:
      return DatabaseStatus::kOk;
    case SERVICE_WORKER_ERROR_FAILED:
    case SERVICE_WORKER_ERROR_ABORT:
      // FAILED is for invalid arguments (e.g. empty key) or database errors.
      // ABORT is for unexpected failures, e.g. because shutdown is in progress.
      // BackgroundFetchDataManager handles both of these the same way.
      return DatabaseStatus::kFailed;
    case SERVICE_WORKER_ERROR_NOT_FOUND:
      // This can also happen for writes, if the ServiceWorkerRegistration has
      // been deleted.
      return DatabaseStatus::kNotFound;
    case SERVICE_WORKER_ERROR_START_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND:
    case SERVICE_WORKER_ERROR_EXISTS:
    case SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_IPC_FAILED:
    case SERVICE_WORKER_ERROR_NETWORK:
    case SERVICE_WORKER_ERROR_SECURITY:
    case SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED:
    case SERVICE_WORKER_ERROR_STATE:
    case SERVICE_WORKER_ERROR_TIMEOUT:
    case SERVICE_WORKER_ERROR_SCRIPT_EVALUATE_FAILED:
    case SERVICE_WORKER_ERROR_DISK_CACHE:
    case SERVICE_WORKER_ERROR_REDUNDANT:
    case SERVICE_WORKER_ERROR_DISALLOWED:
    case SERVICE_WORKER_ERROR_MAX_VALUE:
      break;
  }
  NOTREACHED();
  return DatabaseStatus::kFailed;
}

// Returns whether the response contained in the Background Fetch |request| is
// considered OK. See https://fetch.spec.whatwg.org/#ok-status aka a successful
// 2xx status per https://tools.ietf.org/html/rfc7231#section-6.3.
bool IsOK(const BackgroundFetchRequestInfo& request) {
  int status = request.GetResponseCode();
  return status >= 200 && status < 300;
}

const char kRegistrationKeyPrefix[] = "bgf_registration_";

std::string RegistrationKey(
    const BackgroundFetchRegistrationId& registration_id) {
  return kRegistrationKeyPrefix + registration_id.id();
}

}  // namespace

// A DatabaseTask is an asynchronous "transaction" that needs to read/write the
// Service Worker Database.
//
// Only one DatabaseTask can run at once per StoragePartition, and no other code
// reads/writes Background Fetch keys, so each task effectively has an exclusive
// lock, except that core Service Worker code may delete all keys for a
// ServiceWorkerRegistration or the entire database at any time.
class BackgroundFetchDataManager::DatabaseTask {
 public:
  virtual ~DatabaseTask() = default;

  void Run() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(!data_manager_->database_tasks_.empty());
    DCHECK_EQ(data_manager_->database_tasks_.front().get(), this);
    Start();
  }

 protected:
  explicit DatabaseTask(BackgroundFetchDataManager* data_manager)
      : data_manager_(data_manager) {}

  // The task should begin reading/writing when this is called.
  virtual void Start() = 0;

  // Each task MUST call this once finished, even if exceptions occur, to
  // release their lock and allow the next task to execute.
  void Finished() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(!data_manager_->database_tasks_.empty());
    DCHECK_EQ(data_manager_->database_tasks_.front().get(), this);
    // Keep a reference to |this| on the stack, so |this| lives until |self|
    // goes out of scope instead of being destroyed when |pop| is called.
    std::unique_ptr<DatabaseTask> self(
        std::move(data_manager_->database_tasks_.front()));
    data_manager_->database_tasks_.pop();
    if (!data_manager_->database_tasks_.empty())
      data_manager_->database_tasks_.front()->Run();
  }

  ServiceWorkerContextWrapper* service_worker_context() {
    DCHECK(data_manager_->service_worker_context_);
    return data_manager_->service_worker_context_.get();
  }

 private:
  BackgroundFetchDataManager* data_manager_;  // Owns this.

  DISALLOW_COPY_AND_ASSIGN(DatabaseTask);
};

namespace {

class CreateRegistrationTask : public BackgroundFetchDataManager::DatabaseTask {
 public:
  CreateRegistrationTask(
      BackgroundFetchDataManager* data_manager,
      const BackgroundFetchRegistrationId& registration_id,
      const std::vector<ServiceWorkerFetchRequest>& requests,
      const BackgroundFetchOptions& options,
      BackgroundFetchDataManager::CreateRegistrationCallback callback)
      : DatabaseTask(data_manager),
        registration_id_(registration_id),
        requests_(requests),
        options_(options),
        callback_(std::move(callback)),
        weak_factory_(this) {}

  ~CreateRegistrationTask() override = default;

  void Start() override {
    service_worker_context()->GetRegistrationUserData(
        registration_id_.service_worker_registration_id(),
        {RegistrationKey(registration_id_)},
        base::Bind(&CreateRegistrationTask::DidGetRegistration,
                   weak_factory_.GetWeakPtr()));
  }

 private:
  void DidGetRegistration(const std::vector<std::string>& data,
                          ServiceWorkerStatusCode status) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kNotFound:
        service_worker_context()->StoreRegistrationUserData(
            registration_id_.service_worker_registration_id(),
            registration_id_.origin().GetURL(),
            {
                {RegistrationKey(registration_id_), "TODO VALUE"}
                // TODO(crbug.com/757760): Store requests as well.
            },
            base::Bind(&CreateRegistrationTask::DidStoreRegistration,
                       weak_factory_.GetWeakPtr()));
        return;
      case DatabaseStatus::kOk:
        std::move(callback_).Run(
            blink::mojom::BackgroundFetchError::DUPLICATED_ID);
        Finished();  // Destroys |this|.
        return;
      case DatabaseStatus::kFailed:
        std::move(callback_).Run(
            blink::mojom::BackgroundFetchError::STORAGE_ERROR);
        Finished();  // Destroys |this|.
        return;
    }
  }

  void DidStoreRegistration(ServiceWorkerStatusCode status) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kOk:
        std::move(callback_).Run(blink::mojom::BackgroundFetchError::NONE);
        Finished();  // Destroys |this|.
        return;
      case DatabaseStatus::kFailed:
      case DatabaseStatus::kNotFound:
        std::move(callback_).Run(
            blink::mojom::BackgroundFetchError::STORAGE_ERROR);
        Finished();  // Destroys |this|.
        return;
    }
  }

  BackgroundFetchRegistrationId registration_id_;
  std::vector<ServiceWorkerFetchRequest> requests_;
  BackgroundFetchOptions options_;
  BackgroundFetchDataManager::CreateRegistrationCallback callback_;

  base::WeakPtrFactory<CreateRegistrationTask> weak_factory_;  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(CreateRegistrationTask);
};

class DeleteRegistrationTask : public BackgroundFetchDataManager::DatabaseTask {
 public:
  DeleteRegistrationTask(
      BackgroundFetchDataManager* data_manager,
      const BackgroundFetchRegistrationId& registration_id,
      BackgroundFetchDataManager::DeleteRegistrationCallback callback)
      : DatabaseTask(data_manager),
        registration_id_(registration_id),
        callback_(std::move(callback)),
        weak_factory_(this) {}

  void Start() override {
    service_worker_context()->ClearRegistrationUserData(
        registration_id_.service_worker_registration_id(),
        {
            RegistrationKey(registration_id_)
            // TODO(crbug.com/757760): Delete requests as well.
        },
        base::Bind(&DeleteRegistrationTask::DidDeleteRegistration,
                   weak_factory_.GetWeakPtr()));
  }

 private:
  void DidDeleteRegistration(ServiceWorkerStatusCode status) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kOk:
      case DatabaseStatus::kNotFound:
        std::move(callback_).Run(blink::mojom::BackgroundFetchError::NONE);
        Finished();  // Destroys |this|.
        return;
      case DatabaseStatus::kFailed:
        std::move(callback_).Run(
            blink::mojom::BackgroundFetchError::STORAGE_ERROR);
        Finished();  // Destroys |this|.
        return;
    }
  }

  BackgroundFetchRegistrationId registration_id_;
  BackgroundFetchDataManager::DeleteRegistrationCallback callback_;

  base::WeakPtrFactory<DeleteRegistrationTask> weak_factory_;  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(DeleteRegistrationTask);
};

}  // namespace

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
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : service_worker_context_(std::move(service_worker_context)),
      weak_ptr_factory_(this) {
  // Constructed on the UI thread, then used on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);

  // Store the blob storage context for the given |browser_context|.
  blob_storage_context_ =
      make_scoped_refptr(ChromeBlobStorageContext::GetFor(browser_context));
  DCHECK(blob_storage_context_);
}

BackgroundFetchDataManager::~BackgroundFetchDataManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundFetchDataManager::CreateRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    CreateRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBackgroundFetchPersistence)) {
    AddDatabaseTask(std::make_unique<CreateRegistrationTask>(
        this, registration_id, requests, options, std::move(callback)));
    return;
  }

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
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

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
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = registrations_.find(registration_id);
  DCHECK(iter != registrations_.end());

  RegistrationData* registration_data = iter->second.get();
  registration_data->MarkRequestAsStarted(request, download_guid);
}

void BackgroundFetchDataManager::MarkRequestAsComplete(
    const BackgroundFetchRegistrationId& registration_id,
    BackgroundFetchRequestInfo* request,
    MarkedCompleteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

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
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

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
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBackgroundFetchPersistence)) {
    AddDatabaseTask(std::make_unique<DeleteRegistrationTask>(
        this, registration_id, std::move(callback)));
    return;
  }

  auto iter = registrations_.find(registration_id);
  if (iter == registrations_.end()) {
    std::move(callback).Run(blink::mojom::BackgroundFetchError::INVALID_ID);
    return;
  }

  registrations_.erase(iter);

  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE);
}

void BackgroundFetchDataManager::AddDatabaseTask(
    std::unique_ptr<DatabaseTask> task) {
  database_tasks_.push(std::move(task));
  if (database_tasks_.size() == 1)
    database_tasks_.front()->Run();
}

}  // namespace content
