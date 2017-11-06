// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/storage/database_task.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/background_fetch/background_fetch.mojom.h"
#include "url/origin.h"

namespace storage {
class BlobDataHandle;
}

namespace content {

class BackgroundFetchRequestInfo;
struct BackgroundFetchSettledFetch;
class BrowserContext;
class ChromeBlobStorageContext;
class ServiceWorkerContextWrapper;

// Interface used by the BackgroundFetchDataManager with the
// BackgroundFetchJobController.
// TODO(crbug.com/757760): As part of the persistence work, we should just
// decouple the DataManager from the JobController rather than having this
// clunky interface of unconnected methods.
class BackgroundFetchDatabaseClient {
 public:
  virtual ~BackgroundFetchDatabaseClient() {}

  // Called once the status of the active and completed downloads has been
  // loaded from the database.
  virtual void InitializeRequestStatus(
      int completed_downloads,
      int total_downloads,
      const std::vector<std::string>& outstanding_guids) = 0;
};

// The BackgroundFetchDataManager is a wrapper around persistent storage (the
// Service Worker database), exposing APIs for the read and write queries needed
// for Background Fetch.
//
// There must only be a single instance of this class per StoragePartition, and
// it must only be used on the IO thread, since it relies on there being no
// other code concurrently reading/writing the Background Fetch keys of the same
// Service Worker database (except for deletions, e.g. it's safe for the Service
// Worker code to remove a ServiceWorkerRegistration and all its keys).
//
// Storage schema is documented in storage/README.md
class CONTENT_EXPORT BackgroundFetchDataManager {
 public:
  using NextRequestCallback =
      base::OnceCallback<void(scoped_refptr<BackgroundFetchRequestInfo>)>;
  using MarkedCompleteCallback =
      base::OnceCallback<void(bool /* has_pending_or_active_requests */)>;
  using SettledFetchesCallback = base::OnceCallback<void(
      blink::mojom::BackgroundFetchError,
      bool /* background_fetch_succeeded */,
      std::vector<BackgroundFetchSettledFetch>,
      std::vector<std::unique_ptr<storage::BlobDataHandle>>)>;
  using GetRegistrationCallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError,
                              std::unique_ptr<BackgroundFetchRegistration>)>;

  BackgroundFetchDataManager(
      BrowserContext* browser_context,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);
  ~BackgroundFetchDataManager();

  // Sets the BackgroundFetchDatabaseClient that handles in-progress requests
  // for a registration, and will be notified of relevant changes to the
  // registration data. Must outlive |this| or call SetDatabaseClient(nullptr)
  // in its destructor.
  void SetDatabaseClient(const BackgroundFetchRegistrationId& registration_id,
                         BackgroundFetchDatabaseClient* client);

  BackgroundFetchDatabaseClient* GetDatabaseClientFromUniqueID(
      const std::string& unique_id);

  // Creates and stores a new registration with the given properties. Will
  // invoke the |callback| when the registration has been created, which may
  // fail due to invalid input or storage errors.
  void CreateRegistration(
      const BackgroundFetchRegistrationId& registration_id,
      const std::vector<ServiceWorkerFetchRequest>& requests,
      const BackgroundFetchOptions& options,
      GetRegistrationCallback callback);

  // Get the BackgroundFetchOptions for a registration.
  void GetRegistration(int64_t service_worker_registration_id,
                       const url::Origin& origin,
                       const std::string& developer_id,
                       GetRegistrationCallback callback);

  // Updates the UI values for a Background Fetch registration.
  void UpdateRegistrationUI(
      const std::string& unique_id,
      const std::string& title,
      blink::mojom::BackgroundFetchService::UpdateUICallback callback);

  // Removes the next request, if any, from the pending requests queue, and
  // invokes the |callback| with that request, else a null request.
  void PopNextRequest(const BackgroundFetchRegistrationId& registration_id,
                      NextRequestCallback callback);

  // Marks that the |request|, part of the Background Fetch identified by
  // |registration_id|, has been started as |download_guid|.
  void MarkRequestAsStarted(
      const BackgroundFetchRegistrationId& registration_id,
      BackgroundFetchRequestInfo* request,
      const std::string& download_guid);

  // Marks that the |request|, part of the Background Fetch identified by
  // |registration_id|, has completed.
  void MarkRequestAsComplete(
      const BackgroundFetchRegistrationId& registration_id,
      BackgroundFetchRequestInfo* request,
      MarkedCompleteCallback callback);

  // Reads all settled fetches for the given |registration_id|. Both the Request
  // and Response objects will be initialised based on the stored data. Will
  // invoke the |callback| when the list of fetches has been compiled.
  void GetSettledFetchesForRegistration(
      const BackgroundFetchRegistrationId& registration_id,
      SettledFetchesCallback callback);

  // Marks that the backgroundfetched/backgroundfetchfail/backgroundfetchabort
  // event is being dispatched. It's not possible to call DeleteRegistration at
  // this point as JavaScript may hold a reference to a
  // BackgroundFetchRegistration object and we need to keep the corresponding
  // data around until the last such reference is released (or until shutdown).
  // We can't just move the Background Fetch registration's data to RAM as it
  // might consume too much memory. So instead this step disassociates the
  // |developer_id| from the |unique_id|, so that existing JS objects with a
  // reference to |unique_id| can still access the data, but it can no longer be
  // reached using GetIds or GetRegistration.
  void MarkRegistrationForDeletion(
      const BackgroundFetchRegistrationId& registration_id,
      HandleBackgroundFetchErrorCallback callback);

  // Deletes the registration identified by |registration_id|. Should only be
  // called once the refcount of JavaScript BackgroundFetchRegistration objects
  // referring to this registration drops to zero. Will invoke the |callback|
  // when the registration has been deleted from storage.
  void DeleteRegistration(const BackgroundFetchRegistrationId& registration_id,
                          HandleBackgroundFetchErrorCallback callback);

  // List all Background Fetch registration |developer_id|s for a Service
  // Worker.
  void GetDeveloperIdsForServiceWorker(
      int64_t service_worker_registration_id,
      blink::mojom::BackgroundFetchService::GetDeveloperIdsCallback callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(BackgroundFetchDataManagerTest, Cleanup);
  friend class BackgroundFetchDataManagerTest;
  friend class background_fetch::DatabaseTask;

  class RegistrationData;

  void AddDatabaseTask(std::unique_ptr<background_fetch::DatabaseTask> task);

  void OnDatabaseTaskFinished(background_fetch::DatabaseTask* task);

  // Returns true if not aborted/completed/failed.
  bool IsActive(const BackgroundFetchRegistrationId& registration_id);

  void Cleanup();

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  // The blob storage request with which response information will be stored.
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  // Map from {service_worker_registration_id, origin, developer_id} tuples to
  // the |unique_id|s of active background fetch registrations (not
  // completed/failed/aborted, so there will never be more than one entry for a
  // given key).
  std::map<std::tuple<int64_t, url::Origin, std::string>, std::string>
      active_registration_unique_ids_;

  // Map from the |unique_id|s of known (but possibly inactive) background fetch
  // registrations to their associated data.
  std::map<std::string, std::unique_ptr<RegistrationData>> registrations_;

  // Map from background fetch registration |unique_id|s to the
  // BackgroundFetchDatabaseClient that needs to be notified about changes to
  // the registration.
  base::flat_map<std::string, BackgroundFetchDatabaseClient*> database_clients_;

  // Pending database operations, serialized to ensure consistency.
  // Invariant: the frontmost task, if any, has already been started.
  base::queue<std::unique_ptr<background_fetch::DatabaseTask>> database_tasks_;

  // The |unique_id|s of registrations that have been deactivated since the
  // browser was last started. They will be automatically deleted when the
  // refcount of JavaScript objects that refers to them goes to zero, unless
  // the browser is shutdown first.
  std::set<std::string> ref_counted_unique_ids_;

  base::WeakPtrFactory<BackgroundFetchDataManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchDataManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_
