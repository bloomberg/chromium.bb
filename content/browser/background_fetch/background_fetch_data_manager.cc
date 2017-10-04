// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
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

// Service Worker DB UserData schema
// =================================
// Design doc:
// https://docs.google.com/document/d/1-WPPTP909Gb5PnaBOKP58tPVLw2Fq0Ln-u1EBviIBns/edit
//
// - Each key will be stored twice by the Service Worker DB, once as a
//   "REG_HAS_USER_DATA:", and once as a "REG_USER_DATA:" - see
//   content/browser/service_worker/service_worker_database.cc for details.
// - Integer values are serialized as a string by base::Int*ToString().
//
// key: "bgfetch_registration_" + <std::string 'registration_id'>
// value: <std::string 'serialized content::proto::BackgroundFetchRegistration'>
//
// key: "bgfetch_request_" + <std::string 'registration_id'>
//          + "_" + <int 'request_index'>
// value: <TODO: FetchAPIRequest serialized as a string>
//
// key: "bgfetch_pending_request_"
//          + <int64_t 'registration_creation_microseconds_since_unix_epoch'>
//          + "_" + <std::string 'registration_id'>
//          + "_" + <int 'request_index'>
// value: ""

namespace content {

namespace {

const char kSeparator[] = "_";  // Warning: registration IDs may contain these.
const char kRegistrationKeyPrefix[] = "bgfetch_registration_";
const char kRequestKeyPrefix[] = "bgfetch_request_";
const char kPendingRequestKeyPrefix[] = "bgfetch_pending_request_";

std::string RegistrationKey(
    const BackgroundFetchRegistrationId& registration_id) {
  // Allows looking up a registration by ID.
  return kRegistrationKeyPrefix + registration_id.id();
}

std::string RequestKeyPrefix(
    const BackgroundFetchRegistrationId& registration_id) {
  // Allows looking up all requests within a registration.
  return kRequestKeyPrefix + registration_id.id() + kSeparator;
}

std::string RequestKey(const BackgroundFetchRegistrationId& registration_id,
                       int request_index) {
  // Allows looking up a request by registration ID and index within that.
  return RequestKeyPrefix(registration_id) + base::IntToString(request_index);
}

std::string PendingRequestKeyPrefix(
    int64_t registration_creation_microseconds_since_unix_epoch,
    const BackgroundFetchRegistrationId& registration_id) {
  // These keys are ordered by creation time rather than by registration_id, so
  // the highest priority pending requests can be looked up by fetching the
  // lexicographically smallest keys.
  //
  // Currently (pending crbug.com/741609) registrations within each
  // StoragePartition are prioritised in simple FIFO order by creation time.
  // Since the ordering must survive restarts, wall clock time is used, but that
  // is not monotonically increasing, so the ordering is not exact, and the
  // registration ID is appended to break ties in case the wall clock returns
  // the same values more than once.
  //
  // On Nov 20 2286 17:46:39 the microseconds will transition from 9999999999999
  // to 10000000000000 and pending requests will briefly sort incorrectly.
  return kPendingRequestKeyPrefix +
         base::Int64ToString(
             registration_creation_microseconds_since_unix_epoch) +
         kSeparator + registration_id.id() + kSeparator;
}

std::string PendingRequestKey(
    int64_t registration_creation_microseconds_since_unix_epoch,
    const BackgroundFetchRegistrationId& registration_id,
    int request_index) {
  // In addition to the ordering from PendingRequestKeyPrefix, the requests
  // within each registration should be prioritized according to their index.
  return PendingRequestKeyPrefix(
             registration_creation_microseconds_since_unix_epoch,
             registration_id) +
         base::IntToString(request_index);
}

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
        StoreRegistration();
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

  void StoreRegistration() {
    int64_t registration_creation_microseconds_since_unix_epoch =
        (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds();

    std::vector<std::pair<std::string, std::string>> entries;
    entries.reserve(requests_.size() * 2 + 1);

    // First serialize per-registration (as opposed to per-request) data.
    // TODO(crbug.com/757760): Serialize BackgroundFetchOptions as part of this.
    proto::BackgroundFetchRegistration registration_proto;
    registration_proto.set_creation_microseconds_since_unix_epoch(
        registration_creation_microseconds_since_unix_epoch);
    std::string serialized_registration_proto;
    if (!registration_proto.SerializeToString(&serialized_registration_proto)) {
      // TODO(johnme): Log failures to UMA.
      std::move(callback_).Run(
          blink::mojom::BackgroundFetchError::STORAGE_ERROR);
      Finished();  // Destroys |this|.
      return;
    }
    entries.emplace_back(RegistrationKey(registration_id_),
                         std::move(serialized_registration_proto));

    // Signed integers are used for request indexes to avoid unsigned gotchas.
    for (int i = 0; i < base::checked_cast<int>(requests_.size()); i++) {
      // TODO(crbug.com/757760): Serialize actual values for these entries.
      entries.emplace_back(RequestKey(registration_id_, i),
                           "TODO: Serialize FetchAPIRequest as value");
      entries.emplace_back(
          PendingRequestKey(registration_creation_microseconds_since_unix_epoch,
                            registration_id_, i),
          std::string());
    }

    service_worker_context()->StoreRegistrationUserData(
        registration_id_.service_worker_registration_id(),
        registration_id_.origin().GetURL(), entries,
        base::Bind(&CreateRegistrationTask::DidStoreRegistration,
                   weak_factory_.GetWeakPtr()));
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

  ~DeleteRegistrationTask() override = default;

  void Start() override {
    // Get the registration creation time so we can delete any pending requests.
    service_worker_context()->GetRegistrationUserData(
        registration_id_.service_worker_registration_id(),
        {RegistrationKey(registration_id_)},
        base::Bind(&DeleteRegistrationTask::DidGetRegistration,
                   weak_factory_.GetWeakPtr()));
  }

 private:
  void DidGetRegistration(const std::vector<std::string>& data,
                          ServiceWorkerStatusCode status) {
    std::vector<std::string> prefixes_to_clear = {
        RegistrationKey(registration_id_), RequestKeyPrefix(registration_id_)};

    if (status == SERVICE_WORKER_OK) {
      DCHECK_EQ(1u, data.size());
      proto::BackgroundFetchRegistration registration_proto;
      if (registration_proto.ParseFromString(data[0]) &&
          registration_proto.has_creation_microseconds_since_unix_epoch()) {
        prefixes_to_clear.emplace_back(PendingRequestKeyPrefix(
            registration_proto.creation_microseconds_since_unix_epoch(),
            registration_id_));
      }
    }

    service_worker_context()->ClearRegistrationUserDataByKeyPrefixes(
        registration_id_.service_worker_registration_id(), prefixes_to_clear,
        base::Bind(&DeleteRegistrationTask::DidDeleteRegistration,
                   weak_factory_.GetWeakPtr()));
  }

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

  void SetTitle(const std::string& title) { options_.title = title; }

  const BackgroundFetchOptions& options() const { return options_; }

 private:
  BackgroundFetchOptions options_;

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
}

BackgroundFetchDataManager::~BackgroundFetchDataManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundFetchDataManager::SetListener(
    const BackgroundFetchRegistrationId& registration_id,
    RegistrationListener* listener) {
  if (listener) {
    DCHECK_EQ(0u, listeners_.count(registration_id));
    listeners_[registration_id] = listener;
  } else {
    DCHECK_EQ(1u, listeners_.count(registration_id));
    listeners_.erase(registration_id);
  }
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

void BackgroundFetchDataManager::GetRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    blink::mojom::BackgroundFetchService::GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = registrations_.find(registration_id);
  if (iter == registrations_.end()) {  // Not found.
    std::move(callback).Run(blink::mojom::BackgroundFetchError::INVALID_ID,
                            base::nullopt /* registration */);
    return;
  }

  const BackgroundFetchOptions& options = iter->second->options();

  // Compile the BackgroundFetchRegistration object that will be given to the
  // developer, representing the data associated with the |controller|.
  BackgroundFetchRegistration registration;
  registration.id = iter->first.id();
  registration.icons = options.icons;
  registration.title = options.title;
  registration.download_total = options.download_total;

  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE,
                          registration);
}

void BackgroundFetchDataManager::UpdateRegistrationUI(
    const BackgroundFetchRegistrationId& registration_id,
    const std::string& title,
    blink::mojom::BackgroundFetchService::UpdateUICallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto registrations_iter = registrations_.find(registration_id);
  if (registrations_iter == registrations_.end()) {  // Not found.
    std::move(callback).Run(blink::mojom::BackgroundFetchError::INVALID_ID);
    return;
  }

  // Update stored registration.
  registrations_iter->second->SetTitle(title);

  // Update any active JobController that cached this data for notifications.
  auto listeners_iter = listeners_.find(registration_id);
  if (listeners_iter != listeners_.end())
    listeners_iter->second->UpdateUI(title);

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

void BackgroundFetchDataManager::GetIdsForServiceWorker(
    int64_t service_worker_registration_id,
    blink::mojom::BackgroundFetchService::GetIdsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::vector<std::string> ids;
  for (const auto& entry : registrations_) {
    const BackgroundFetchRegistrationId& registration_id = entry.first;
    if (service_worker_registration_id ==
        registration_id.service_worker_registration_id()) {
      ids.emplace_back(registration_id.id());
    }
  }

  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE, ids);
}

void BackgroundFetchDataManager::AddDatabaseTask(
    std::unique_ptr<DatabaseTask> task) {
  database_tasks_.push(std::move(task));
  if (database_tasks_.size() == 1)
    database_tasks_.front()->Run();
}

}  // namespace content
