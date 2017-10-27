// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
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
#include "content/public/browser/browser_thread.h"
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
// key: "bgfetch_active_registration_unique_id_" + <std::string 'developer_id'>
// value: <std::string 'unique_id'>
//
// key: "bgfetch_registration_" + <std::string 'unique_id'>
// value: <std::string 'serialized content::proto::BackgroundFetchRegistration'>
//
// key: "bgfetch_request_" + <std::string 'registration unique_id'>
//          + "_" + <int 'request_index'>
// value: <TODO: FetchAPIRequest serialized as a string>
//
// key: "bgfetch_pending_request_"
//          + <int64_t 'registration_creation_microseconds_since_unix_epoch'>
//          + "_" + <std::string 'registration unique_id'>
//          + "_" + <int 'request_index'>
// value: ""

namespace content {

namespace {

// Warning: registration |developer_id|s may contain kSeparator characters.
const char kSeparator[] = "_";

const char kActiveRegistrationUniqueIdKeyPrefix[] =
    "bgfetch_active_registration_unique_id_";
const char kRegistrationKeyPrefix[] = "bgfetch_registration_";
const char kRequestKeyPrefix[] = "bgfetch_request_";
const char kPendingRequestKeyPrefix[] = "bgfetch_pending_request_";

std::string ActiveRegistrationUniqueIdKey(const std::string& developer_id) {
  // Allows looking up the active registration's |unique_id| by |developer_id|.
  // Registrations are active from creation up until completed/failed/aborted.
  // These database entries correspond to the active background fetches map:
  // https://wicg.github.io/background-fetch/#service-worker-registration-active-background-fetches
  return kActiveRegistrationUniqueIdKeyPrefix + developer_id;
}

std::string RegistrationKey(const std::string& unique_id) {
  // Allows looking up a registration by |unique_id|.
  return kRegistrationKeyPrefix + unique_id;
}

std::string RequestKeyPrefix(const std::string& unique_id) {
  // Allows looking up all requests within a registration.
  return kRequestKeyPrefix + unique_id + kSeparator;
}

std::string RequestKey(const std::string& unique_id, int request_index) {
  // Allows looking up a request by registration id and index within that.
  return RequestKeyPrefix(unique_id) + base::IntToString(request_index);
}

std::string PendingRequestKeyPrefix(
    int64_t registration_creation_microseconds_since_unix_epoch,
    const std::string& unique_id) {
  // These keys are ordered by the registration's creation time rather than by
  // its |unique_id|, so that the highest priority pending requests in FIFO
  // order can be looked up by fetching the lexicographically smallest keys.
  // https://crbug.com/741609 may introduce more advanced prioritisation.
  //
  // Since the ordering must survive restarts, wall clock time is used, but that
  // is not monotonically increasing, so the ordering is not exact, and the
  // |unique_id| is appended to break ties in case the wall clock returns the
  // same values more than once.
  //
  // On Nov 20 2286 17:46:39 the microseconds will transition from 9999999999999
  // to 10000000000000 and pending requests will briefly sort incorrectly.
  return kPendingRequestKeyPrefix +
         base::Int64ToString(
             registration_creation_microseconds_since_unix_epoch) +
         kSeparator + unique_id + kSeparator;
}

std::string PendingRequestKey(
    int64_t registration_creation_microseconds_since_unix_epoch,
    const std::string& unique_id,
    int request_index) {
  // In addition to the ordering from PendingRequestKeyPrefix, the requests
  // within each registration should be prioritized according to their index.
  return PendingRequestKeyPrefix(
             registration_creation_microseconds_since_unix_epoch, unique_id) +
         base::IntToString(request_index);
}

void IgnoreError(blink::mojom::BackgroundFetchError) {}

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

#if DCHECK_IS_ON()
// Checks that the |ActiveRegistrationUniqueIdKey| either does not exist, or is
// associated with a different |unique_id| than the given one which should have
// been already marked for deletion.
void DCheckRegistrationNotActive(const std::string& unique_id,
                                 const std::vector<std::string>& data,
                                 ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
      DCHECK_EQ(1u, data.size());
      DCHECK_NE(unique_id, data[0])
          << "Must call MarkRegistrationForDeletion before DeleteRegistration";
      return;
    case DatabaseStatus::kFailed:
      return;  // TODO(johnme): Consider logging failure to UMA.
    case DatabaseStatus::kNotFound:
      return;
  }
}
#endif  // DCHECK_IS_ON()

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
      : data_manager_(data_manager) {
    DCHECK(data_manager_);
  }

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

  Controller* GetController(const std::string& unique_id) {
    auto iter = data_manager_->controllers_.find(unique_id);
    return iter == data_manager_->controllers_.end() ? nullptr : iter->second;
  }

  void AddDatabaseTask(std::unique_ptr<DatabaseTask> task) {
    data_manager_->AddDatabaseTask(std::move(task));
  }

  ServiceWorkerContextWrapper* service_worker_context() {
    DCHECK(data_manager_->service_worker_context_);
    return data_manager_->service_worker_context_.get();
  }

  std::set<std::string>& ref_counted_unique_ids() {
    return data_manager_->ref_counted_unique_ids_;
  }

  BackgroundFetchDataManager* data_manager() { return data_manager_; }

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
      blink::mojom::BackgroundFetchService::FetchCallback callback)
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
        {ActiveRegistrationUniqueIdKey(registration_id_.developer_id())},
        base::Bind(&CreateRegistrationTask::DidGetUniqueId,
                   weak_factory_.GetWeakPtr()));
  }

 private:
  void DidGetUniqueId(const std::vector<std::string>& data,
                      ServiceWorkerStatusCode status) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kNotFound:
        StoreRegistration();
        return;
      case DatabaseStatus::kOk:
        // Can't create a registration since there is already an active
        // registration with the same |developer_id|. It must be deactivated
        // (completed/failed/aborted) first.
        std::move(callback_).Run(
            blink::mojom::BackgroundFetchError::DUPLICATED_DEVELOPER_ID,
            base::nullopt /* registration */);
        Finished();  // Destroys |this|.
        return;
      case DatabaseStatus::kFailed:
        std::move(callback_).Run(
            blink::mojom::BackgroundFetchError::STORAGE_ERROR,
            base::nullopt /* registration */);
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
    registration_proto.set_unique_id(registration_id_.unique_id());
    registration_proto.set_developer_id(registration_id_.developer_id());
    registration_proto.set_creation_microseconds_since_unix_epoch(
        registration_creation_microseconds_since_unix_epoch);
    std::string serialized_registration_proto;
    if (!registration_proto.SerializeToString(&serialized_registration_proto)) {
      // TODO(johnme): Log failures to UMA.
      std::move(callback_).Run(
          blink::mojom::BackgroundFetchError::STORAGE_ERROR,
          base::nullopt /* registration */);
      Finished();  // Destroys |this|.
      return;
    }
    entries.emplace_back(
        ActiveRegistrationUniqueIdKey(registration_id_.developer_id()),
        registration_id_.unique_id());
    entries.emplace_back(RegistrationKey(registration_id_.unique_id()),
                         std::move(serialized_registration_proto));

    // Signed integers are used for request indexes to avoid unsigned gotchas.
    for (int i = 0; i < base::checked_cast<int>(requests_.size()); i++) {
      // TODO(crbug.com/757760): Serialize actual values for these entries.
      entries.emplace_back(RequestKey(registration_id_.unique_id(), i),
                           "TODO: Serialize FetchAPIRequest as value");
      entries.emplace_back(
          PendingRequestKey(registration_creation_microseconds_since_unix_epoch,
                            registration_id_.unique_id(), i),
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
        break;
      case DatabaseStatus::kFailed:
      case DatabaseStatus::kNotFound:
        std::move(callback_).Run(
            blink::mojom::BackgroundFetchError::STORAGE_ERROR,
            base::nullopt /* registration */);
        Finished();  // Destroys |this|.
        return;
    }

    BackgroundFetchRegistration registration;
    registration.developer_id = registration_id_.developer_id();
    registration.unique_id = registration_id_.unique_id();
    registration.icons = options_.icons;
    registration.title = options_.title;
    // TODO(crbug.com/774054): Uploads are not yet supported.
    registration.upload_total = 0;
    registration.uploaded = 0;
    registration.download_total = options_.download_total;
    registration.downloaded = 0;

    std::move(callback_).Run(blink::mojom::BackgroundFetchError::NONE,
                             std::move(registration));
    Finished();  // Destroys |this|.
  }

  BackgroundFetchRegistrationId registration_id_;
  std::vector<ServiceWorkerFetchRequest> requests_;
  BackgroundFetchOptions options_;
  blink::mojom::BackgroundFetchService::FetchCallback callback_;

  base::WeakPtrFactory<CreateRegistrationTask> weak_factory_;  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(CreateRegistrationTask);
};

class MarkRegistrationForDeletionTask
    : public BackgroundFetchDataManager::DatabaseTask {
 public:
  MarkRegistrationForDeletionTask(
      BackgroundFetchDataManager* data_manager,
      const BackgroundFetchRegistrationId& registration_id,
      bool aborted,
      BackgroundFetchDataManager::HandleBackgroundFetchErrorCallback callback)
      : DatabaseTask(data_manager),
        registration_id_(registration_id),
        aborted_(aborted),
        callback_(std::move(callback)),
        weak_factory_(this) {}

  ~MarkRegistrationForDeletionTask() override = default;

  void Start() override {
    // Look up if there is already an active |unique_id| entry for this
    // |developer_id|.
    service_worker_context()->GetRegistrationUserData(
        registration_id_.service_worker_registration_id(),
        {ActiveRegistrationUniqueIdKey(registration_id_.developer_id()),
         RegistrationKey(registration_id_.unique_id())},
        base::Bind(&MarkRegistrationForDeletionTask::DidGetActiveUniqueId,
                   weak_factory_.GetWeakPtr()));
  }

 private:
  void DidGetActiveUniqueId(const std::vector<std::string>& data,
                            ServiceWorkerStatusCode status) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kOk:
        break;
      case DatabaseStatus::kNotFound:
        std::move(callback_).Run(
            blink::mojom::BackgroundFetchError::INVALID_ID);
        Finished();  // Destroys |this|.
        return;
      case DatabaseStatus::kFailed:
        std::move(callback_).Run(
            blink::mojom::BackgroundFetchError::STORAGE_ERROR);
        Finished();  // Destroys |this|.
        return;
    }

    DCHECK_EQ(2u, data.size());

    // If the |unique_id| does not match, then the registration identified by
    // |registration_id_.unique_id()| was already deactivated.
    if (data[0] != registration_id_.unique_id()) {
      std::move(callback_).Run(blink::mojom::BackgroundFetchError::INVALID_ID);
      Finished();  // Destroys |this|.
      return;
    }

    proto::BackgroundFetchRegistration registration_proto;
    if (registration_proto.ParseFromString(data[1]) &&
        registration_proto.has_creation_microseconds_since_unix_epoch()) {
      // Mark registration as no longer active. Also deletes pending request
      // keys, since those are globally sorted and requests within deactivated
      // registrations are no longer eligible to be started. Pending request
      // keys are not required by GetRegistration.
      service_worker_context()->ClearRegistrationUserDataByKeyPrefixes(
          registration_id_.service_worker_registration_id(),
          {ActiveRegistrationUniqueIdKey(registration_id_.developer_id()),
           PendingRequestKeyPrefix(
               registration_proto.creation_microseconds_since_unix_epoch(),
               registration_id_.unique_id())},
          base::Bind(&MarkRegistrationForDeletionTask::DidDeactivate,
                     weak_factory_.GetWeakPtr()));
    } else {
      NOTREACHED() << "Database is corrupt";  // TODO(johnme): Nuke it.
    }
  }

  void DidDeactivate(ServiceWorkerStatusCode status) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kOk:
      case DatabaseStatus::kNotFound:
        break;
      case DatabaseStatus::kFailed:
        std::move(callback_).Run(
            blink::mojom::BackgroundFetchError::STORAGE_ERROR);
        Finished();  // Destroys |this|.
        return;
    }

    // If CleanupTask runs after this, it shouldn't clean up the
    // |unique_id| as there may still be JavaScript references to it.
    ref_counted_unique_ids().emplace(registration_id_.unique_id());

    if (aborted_) {
      auto* controller = GetController(registration_id_.unique_id());
      if (controller)
        controller->Abort();
    }

    std::move(callback_).Run(blink::mojom::BackgroundFetchError::NONE);
    Finished();  // Destroys |this|.
  }

  BackgroundFetchRegistrationId registration_id_;
  bool aborted_;
  BackgroundFetchDataManager::HandleBackgroundFetchErrorCallback callback_;

  base::WeakPtrFactory<MarkRegistrationForDeletionTask>
      weak_factory_;  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(MarkRegistrationForDeletionTask);
};

class DeleteRegistrationTask : public BackgroundFetchDataManager::DatabaseTask {
 public:
  DeleteRegistrationTask(
      BackgroundFetchDataManager* data_manager,
      int64_t service_worker_registration_id,
      const std::string& unique_id,
      BackgroundFetchDataManager::HandleBackgroundFetchErrorCallback callback)
      : DatabaseTask(data_manager),
        service_worker_registration_id_(service_worker_registration_id),
        unique_id_(unique_id),
        callback_(std::move(callback)),
        weak_factory_(this) {}

  ~DeleteRegistrationTask() override = default;

  void Start() override {
#if DCHECK_IS_ON()
    // Get the registration |developer_id| to check it was deactivated.
    service_worker_context()->GetRegistrationUserData(
        service_worker_registration_id_, {RegistrationKey(unique_id_)},
        base::Bind(&DeleteRegistrationTask::DidGetRegistration,
                   weak_factory_.GetWeakPtr()));
#else
    DidGetRegistration({}, SERVICE_WORKER_OK);
#endif  // DCHECK_IS_ON()
  }

 private:
  void DidGetRegistration(const std::vector<std::string>& data,
                          ServiceWorkerStatusCode status) {
#if DCHECK_IS_ON()
    if (ToDatabaseStatus(status) == DatabaseStatus::kOk) {
      DCHECK_EQ(1u, data.size());
      proto::BackgroundFetchRegistration registration_proto;
      if (registration_proto.ParseFromString(data[0]) &&
          registration_proto.has_developer_id()) {
        service_worker_context()->GetRegistrationUserData(
            service_worker_registration_id_,
            {ActiveRegistrationUniqueIdKey(registration_proto.developer_id())},
            base::Bind(&DCheckRegistrationNotActive, unique_id_));
      } else {
        NOTREACHED() << "Database is corrupt";  // TODO(johnme): Nuke it.
      }
    } else {
      // TODO(johnme): Log failure to UMA.
    }
#endif  // DCHECK_IS_ON()

    service_worker_context()->ClearRegistrationUserDataByKeyPrefixes(
        service_worker_registration_id_,
        {RegistrationKey(unique_id_), RequestKeyPrefix(unique_id_)},
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

  int64_t service_worker_registration_id_;
  std::string unique_id_;
  BackgroundFetchDataManager::HandleBackgroundFetchErrorCallback callback_;

  base::WeakPtrFactory<DeleteRegistrationTask> weak_factory_;  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(DeleteRegistrationTask);
};

// Deletes inactive registrations marked for deletion.
// TODO(johnme): Log failed deletions to UMA.
class CleanupTask : public BackgroundFetchDataManager::DatabaseTask {
 public:
  CleanupTask(BackgroundFetchDataManager* data_manager)
      : DatabaseTask(data_manager), weak_factory_(this) {}

  ~CleanupTask() override = default;

  void Start() override {
    service_worker_context()->GetUserDataForAllRegistrationsByKeyPrefix(
        kRegistrationKeyPrefix, base::Bind(&CleanupTask::DidGetRegistrations,
                                           weak_factory_.GetWeakPtr()));
  }

 private:
  void DidGetRegistrations(
      const std::vector<std::pair<int64_t, std::string>>& registration_data,
      ServiceWorkerStatusCode status) {
    if (ToDatabaseStatus(status) != DatabaseStatus::kOk ||
        registration_data.empty()) {
      Finished();  // Destroys |this|.
      return;
    }

    service_worker_context()->GetUserDataForAllRegistrationsByKeyPrefix(
        kActiveRegistrationUniqueIdKeyPrefix,
        base::Bind(&CleanupTask::DidGetActiveUniqueIds,
                   weak_factory_.GetWeakPtr(), registration_data));
  }

 private:
  void DidGetActiveUniqueIds(
      const std::vector<std::pair<int64_t, std::string>>& registration_data,
      const std::vector<std::pair<int64_t, std::string>>& active_unique_id_data,
      ServiceWorkerStatusCode status) {
    switch (ToDatabaseStatus(status)) {
      case DatabaseStatus::kOk:
      case DatabaseStatus::kNotFound:
        break;
      case DatabaseStatus::kFailed:
        Finished();  // Destroys |this|.
        return;
    }

    std::vector<std::string> active_unique_id_vector;
    active_unique_id_vector.reserve(active_unique_id_data.size());
    for (const auto& entry : active_unique_id_data)
      active_unique_id_vector.push_back(entry.second);
    base::flat_set<std::string> active_unique_ids(
        std::move(active_unique_id_vector));

    for (const auto& entry : registration_data) {
      int64_t service_worker_registration_id = entry.first;
      proto::BackgroundFetchRegistration registration_proto;
      if (registration_proto.ParseFromString(entry.second)) {
        if (registration_proto.has_unique_id()) {
          const std::string& unique_id = registration_proto.unique_id();
          if (!active_unique_ids.count(unique_id) &&
              !ref_counted_unique_ids().count(unique_id)) {
            // This |unique_id| can be safely cleaned up. Re-use
            // DeleteRegistrationTask for the actual deletion logic.
            AddDatabaseTask(std::make_unique<DeleteRegistrationTask>(
                data_manager(), service_worker_registration_id, unique_id,
                base::BindOnce(&IgnoreError)));
          }
        }
      }
    }

    Finished();  // Destroys |this|.
    return;
  }

  base::WeakPtrFactory<CleanupTask> weak_factory_;  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(CleanupTask);
};

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

  uint64_t GetDownloaded(Controller* controller) {
    return complete_requests_downloaded_bytes_ +
           (controller ? controller->GetInProgressDownloadedBytes() : 0);
  }

  int GetTotalNumberOfRequests() const {
    return pending_requests_.size() + active_requests_.size() +
           completed_requests_.size();
  }

 private:
  BackgroundFetchRegistrationId registration_id_;
  BackgroundFetchOptions options_;
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
    AddDatabaseTask(std::make_unique<CleanupTask>(this));
  }
}

BackgroundFetchDataManager::~BackgroundFetchDataManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundFetchDataManager::SetController(
    const BackgroundFetchRegistrationId& registration_id,
    Controller* controller) {
  if (controller) {
    DCHECK_EQ(0u, controllers_.count(registration_id.unique_id()));
    controllers_.emplace(registration_id.unique_id(), controller);
  } else {
    DCHECK_EQ(1u, controllers_.count(registration_id.unique_id()));
    controllers_.erase(registration_id.unique_id());
  }
}

void BackgroundFetchDataManager::CreateRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    blink::mojom::BackgroundFetchService::FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBackgroundFetchPersistence)) {
    AddDatabaseTask(std::make_unique<CreateRegistrationTask>(
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
        blink::mojom::BackgroundFetchError::DUPLICATED_DEVELOPER_ID,
        base::nullopt);
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
    blink::mojom::BackgroundFetchService::GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto developer_id_tuple =
      std::make_tuple(service_worker_registration_id, origin, developer_id);

  auto iter = active_registration_unique_ids_.find(developer_id_tuple);
  if (iter == active_registration_unique_ids_.end()) {
    std::move(callback).Run(blink::mojom::BackgroundFetchError::INVALID_ID,
                            base::nullopt /* registration */);
    return;
  }

  const std::string& unique_id = iter->second;

  DCHECK_EQ(1u, registrations_.count(unique_id));
  RegistrationData* data = registrations_[unique_id].get();

  auto controllers_iter = controllers_.find(unique_id);
  Controller* controller = controllers_iter != controllers_.end()
                               ? controllers_iter->second
                               : nullptr;

  // Compile the BackgroundFetchRegistration object for the developer.
  BackgroundFetchRegistration registration;
  registration.developer_id = developer_id;
  registration.unique_id = unique_id;
  registration.icons = data->options().icons;
  registration.title = data->options().title;
  // TODO(crbug.com/774054): Uploads are not yet supported.
  registration.upload_total = 0;
  registration.uploaded = 0;
  registration.download_total = data->options().download_total;
  registration.downloaded = data->GetDownloaded(controller);

  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE,
                          registration);
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

  const BackgroundFetchRegistrationId& registration_id =
      registrations_iter->second->registration_id();

  auto controllers_iter = controllers_.find(unique_id);

  // The registration must a) still be active, or b) have completed/failed (not
  // aborted) with the waitUntil promise from that event not yet resolved. The
  // latter case can be detected because the JobController will be kept alive
  // until the waitUntil promise is resolved.
  if (!IsActive(registration_id) && controllers_iter == controllers_.end()) {
    std::move(callback).Run(blink::mojom::BackgroundFetchError::INVALID_ID);
    return;
  }

  // Update stored registration.
  registrations_iter->second->SetTitle(title);

  // Update any active JobController that cached this data for notifications.
  if (controllers_iter != controllers_.end())
    controllers_iter->second->UpdateUI(title);

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

void BackgroundFetchDataManager::MarkRegistrationForDeletion(
    const BackgroundFetchRegistrationId& registration_id,
    bool aborted,
    HandleBackgroundFetchErrorCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBackgroundFetchPersistence)) {
    AddDatabaseTask(std::make_unique<MarkRegistrationForDeletionTask>(
        this, registration_id, aborted, std::move(callback)));
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

  if (aborted) {
    // Terminate any active JobController's downloads.
    auto controller_iter = controllers_.find(registration_id.unique_id());
    if (controller_iter != controllers_.end())
      controller_iter->second->Abort();
  }

  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE);
}

void BackgroundFetchDataManager::DeleteRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    HandleBackgroundFetchErrorCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBackgroundFetchPersistence)) {
    AddDatabaseTask(std::make_unique<DeleteRegistrationTask>(
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
    blink::mojom::BackgroundFetchService::GetDeveloperIdsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::vector<std::string> developer_ids;
  for (const auto& entry : active_registration_unique_ids_) {
    if (service_worker_registration_id == std::get<0>(entry.first))
      developer_ids.emplace_back(std::get<2>(entry.first));
  }

  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE,
                          developer_ids);
}

int BackgroundFetchDataManager::GetNumberOfRequestsForRegistration(
    const BackgroundFetchRegistrationId& registration_id) {
  return registrations_[registration_id.unique_id()]
      ->GetTotalNumberOfRequests();
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
    std::unique_ptr<DatabaseTask> task) {
  database_tasks_.push(std::move(task));
  if (database_tasks_.size() == 1)
    database_tasks_.front()->Run();
}

}  // namespace content
