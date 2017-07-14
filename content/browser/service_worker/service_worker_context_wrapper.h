// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/observer_list_threadsafe.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/common/content_export.h"
#include "content/common/worker_url_loader_factory_provider.mojom.h"
#include "content/public/browser/service_worker_context.h"

namespace base {
class FilePath;
class SingleThreadTaskRunner;
}

namespace storage {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

namespace content {

class BrowserContext;
class ChromeBlobStorageContext;
class ResourceContext;
class ServiceWorkerContextObserver;
class StoragePartitionImpl;
class URLLoaderFactoryGetter;

// A refcounted wrapper class for our core object. Higher level content lib
// classes keep references to this class on mutliple threads. The inner core
// instance is strictly single threaded and is not refcounted, the core object
// is what is used internally in the service worker lib.
class CONTENT_EXPORT ServiceWorkerContextWrapper
    : NON_EXPORTED_BASE(public ServiceWorkerContext),
      public ServiceWorkerContextCoreObserver,
      public base::RefCountedThreadSafe<ServiceWorkerContextWrapper> {
 public:
  using StatusCallback = base::Callback<void(ServiceWorkerStatusCode)>;
  using BoolCallback = base::Callback<void(bool)>;
  using FindRegistrationCallback =
      ServiceWorkerStorage::FindRegistrationCallback;
  using GetRegistrationsInfosCallback =
      ServiceWorkerStorage::GetRegistrationsInfosCallback;
  using GetUserDataCallback = ServiceWorkerStorage::GetUserDataCallback;
  using GetUserDataForAllRegistrationsCallback =
      ServiceWorkerStorage::GetUserDataForAllRegistrationsCallback;

  ServiceWorkerContextWrapper(BrowserContext* browser_context);

  // Init and Shutdown are for use on the UI thread when the profile,
  // storagepartition is being setup and torn down.
  // |blob_context| and |url_loader_factory_getter| are used only
  // when IsServicificationEnabled is true.
  void Init(const base::FilePath& user_data_directory,
            storage::QuotaManagerProxy* quota_manager_proxy,
            storage::SpecialStoragePolicy* special_storage_policy,
            ChromeBlobStorageContext* blob_context,
            URLLoaderFactoryGetter* url_loader_factory_getter);
  void Shutdown();

  // Must be called on the IO thread.
  void InitializeResourceContext(ResourceContext* resource_context);

  // Deletes all files on disk and restarts the system asynchronously. This
  // leaves the system in a disabled state until it's done. This should be
  // called on the IO thread.
  void DeleteAndStartOver();

  // The StoragePartition should only be used on the UI thread.
  // Can be null before/during init and during/after shutdown.
  StoragePartitionImpl* storage_partition() const;

  void set_storage_partition(StoragePartitionImpl* storage_partition);

  // The ResourceContext for the associated BrowserContext. This should only
  // be accessed on the IO thread, and can be null during initialization and
  // shutdown.
  ResourceContext* resource_context();

  // The process manager can be used on either UI or IO.
  ServiceWorkerProcessManager* process_manager() {
    return process_manager_.get();
  }

  // ServiceWorkerContextCoreObserver implementation:
  void OnRegistrationStored(int64_t registration_id,
                            const GURL& pattern) override;

  // ServiceWorkerContext implementation:
  void AddObserver(ServiceWorkerContextObserver* observer) override;
  void RemoveObserver(ServiceWorkerContextObserver* observer) override;
  void RegisterServiceWorker(const GURL& pattern,
                             const GURL& script_url,
                             const ResultCallback& continuation) override;
  void UnregisterServiceWorker(const GURL& pattern,
                               const ResultCallback& continuation) override;
  void GetAllOriginsInfo(const GetUsageInfoCallback& callback) override;
  void DeleteForOrigin(const GURL& origin,
                       const ResultCallback& callback) override;
  void CheckHasServiceWorker(
      const GURL& url,
      const GURL& other_url,
      const CheckHasServiceWorkerCallback& callback) override;
  void CountExternalRequestsForTest(
      const GURL& url,
      const CountExternalRequestsCallback& callback) override;
  void StopAllServiceWorkersForOrigin(const GURL& origin) override;
  void ClearAllServiceWorkersForTest(const base::Closure& callback) override;
  bool StartingExternalRequest(int64_t service_worker_version_id,
                               const std::string& request_uuid) override;
  bool FinishedExternalRequest(int64_t service_worker_version_id,
                               const std::string& request_uuid) override;
  void StartServiceWorkerForNavigationHint(
      const GURL& document_url,
      const StartServiceWorkerForNavigationHintCallback& callback) override;
  void StartActiveWorkerForPattern(const GURL& pattern,
                                   StartActiveWorkerCallback info_callback,
                                   base::OnceClosure failure_callback) override;

  // These methods must only be called from the IO thread.
  ServiceWorkerRegistration* GetLiveRegistration(int64_t registration_id);
  ServiceWorkerVersion* GetLiveVersion(int64_t version_id);
  std::vector<ServiceWorkerRegistrationInfo> GetAllLiveRegistrationInfo();
  std::vector<ServiceWorkerVersionInfo> GetAllLiveVersionInfo();

  // Must be called from the IO thread.
  void HasMainFrameProviderHost(const GURL& origin,
                                const BoolCallback& callback) const;

  // Returns all render process ids and frame ids for the given |origin|.
  std::unique_ptr<
      std::vector<std::pair<int /* render process id */, int /* frame id */>>>
  GetProviderHostIds(const GURL& origin) const;

  // Returns the registration whose scope longest matches |document_url|. It is
  // guaranteed that the returned registration has the activated worker.
  //
  //  - If the registration is not found, returns ERROR_NOT_FOUND.
  //  - If the registration has neither the waiting version nor the active
  //    version, returns ERROR_NOT_FOUND.
  //  - If the registration does not have the active version but has the waiting
  //    version, activates the waiting version and runs |callback| when it is
  //    activated.
  //
  // Must be called from the IO thread.
  void FindReadyRegistrationForDocument(
      const GURL& document_url,
      const FindRegistrationCallback& callback);

  // Returns the registration for |scope|. It is guaranteed that the returned
  // registration has the activated worker.
  //
  //  - If the registration is not found, returns ERROR_NOT_FOUND.
  //  - If the registration has neither the waiting version nor the active
  //    version, returns ERROR_NOT_FOUND.
  //  - If the registration does not have the active version but has the waiting
  //    version, activates the waiting version and runs |callback| when it is
  //    activated.
  //
  // Must be called from the IO thread.
  void FindReadyRegistrationForPattern(
      const GURL& scope,
      const FindRegistrationCallback& callback);

  // Returns the registration for |registration_id|. It is guaranteed that the
  // returned registration has the activated worker.
  //
  //  - If the registration is not found, returns ERROR_NOT_FOUND.
  //  - If the registration has neither the waiting version nor the active
  //    version, returns ERROR_NOT_FOUND.
  //  - If the registration does not have the active version but has the waiting
  //    version, activates the waiting version and runs |callback| when it is
  //    activated.
  //
  // Must be called from the IO thread.
  void FindReadyRegistrationForId(int64_t registration_id,
                                  const GURL& origin,
                                  const FindRegistrationCallback& callback);

  // Returns the registration for |registration_id|. It is guaranteed that the
  // returned registration has the activated worker.
  //
  // Generally |FindReadyRegistrationForId| should be used to look up a
  // registration by |registration_id| since it's more efficient. But if a
  // |registration_id| is all that is available this method can be used instead.
  //
  //  - If the registration is not found, returns ERROR_NOT_FOUND.
  //  - If the registration has neither the waiting version nor the active
  //    version, returns ERROR_NOT_FOUND.
  //  - If the registration does not have the active version but has the waiting
  //    version, activates the waiting version and runs |callback| when it is
  //    activated.
  //
  // Must be called from the IO thread.
  void FindReadyRegistrationForIdOnly(int64_t registration_id,
                                      const FindRegistrationCallback& callback);

  // All these methods must be called from the IO thread.
  void GetAllRegistrations(const GetRegistrationsInfosCallback& callback);
  void GetRegistrationUserData(int64_t registration_id,
                               const std::vector<std::string>& keys,
                               const GetUserDataCallback& callback);
  void GetRegistrationUserDataByKeyPrefix(int64_t registration_id,
                                          const std::string& key_prefix,
                                          const GetUserDataCallback& callback);
  void StoreRegistrationUserData(
      int64_t registration_id,
      const GURL& origin,
      const std::vector<std::pair<std::string, std::string>>& key_value_pairs,
      const StatusCallback& callback);
  void ClearRegistrationUserData(int64_t registration_id,
                                 const std::vector<std::string>& keys,
                                 const StatusCallback& callback);
  void GetUserDataForAllRegistrations(
      const std::string& key,
      const GetUserDataForAllRegistrationsCallback& callback);
  void GetUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix,
      const GetUserDataForAllRegistrationsCallback& callback);

  // This function can be called from any thread, but the callback will always
  // be called on the UI thread.
  void StartServiceWorker(const GURL& pattern, const StatusCallback& callback);

  // This function can be called from any thread.
  void SkipWaitingWorker(const GURL& pattern);

  // These methods can be called from any thread.
  void UpdateRegistration(const GURL& pattern);
  void SetForceUpdateOnPageLoad(bool force_update_on_page_load);
  void AddObserver(ServiceWorkerContextCoreObserver* observer);
  void RemoveObserver(ServiceWorkerContextCoreObserver* observer);

  bool is_incognito() const { return is_incognito_; }

  // Must be called from the IO thread.
  bool OriginHasForeignFetchRegistrations(const GURL& origin);

  // Binds the ServiceWorkerWorkerClient of a dedicated (or shared) worker to
  // the parent frame's ServiceWorkerProviderHost. (This is used only when
  // off-main-thread-fetch is enabled.)
  void BindWorkerFetchContext(
      int render_process_id,
      int service_worker_provider_id,
      mojom::ServiceWorkerWorkerClientAssociatedPtrInfo client_ptr_info);

 private:
  friend class BackgroundSyncManagerTest;
  friend class base::RefCountedThreadSafe<ServiceWorkerContextWrapper>;
  friend class EmbeddedWorkerTestHelper;
  friend class EmbeddedWorkerBrowserTest;
  friend class ForeignFetchRequestHandler;
  friend class ServiceWorkerDispatcherHost;
  friend class ServiceWorkerInternalsUI;
  friend class ServiceWorkerNavigationHandleCore;
  friend class ServiceWorkerProcessManager;
  friend class ServiceWorkerRequestHandler;
  friend class ServiceWorkerVersionBrowserTest;
  friend class MockServiceWorkerContextWrapper;

  ~ServiceWorkerContextWrapper() override;

  void InitInternal(
      const base::FilePath& user_data_directory,
      std::unique_ptr<ServiceWorkerDatabaseTaskManager> database_task_manager,
      const scoped_refptr<base::SingleThreadTaskRunner>& disk_cache_thread,
      storage::QuotaManagerProxy* quota_manager_proxy,
      storage::SpecialStoragePolicy* special_storage_policy,
      ChromeBlobStorageContext* blob_context,
      URLLoaderFactoryGetter* url_loader_factory_getter);
  void ShutdownOnIO();

  void DidFindRegistrationForFindReady(
      const FindRegistrationCallback& callback,
      ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void OnStatusChangedForFindReadyRegistration(
      const FindRegistrationCallback& callback,
      scoped_refptr<ServiceWorkerRegistration> registration);

  void DidDeleteAndStartOver(ServiceWorkerStatusCode status);

  void DidGetAllRegistrationsForGetAllOrigins(
      const GetUsageInfoCallback& callback,
      ServiceWorkerStatusCode status,
      const std::vector<ServiceWorkerRegistrationInfo>& registrations);

  void DidCheckHasServiceWorker(const CheckHasServiceWorkerCallback& callback,
                                content::ServiceWorkerCapability status);

  void DidFindRegistrationForUpdate(
      ServiceWorkerStatusCode status,
      scoped_refptr<content::ServiceWorkerRegistration> registration);

  void StartServiceWorkerForNavigationHintOnIO(
      const GURL& document_url,
      const StartServiceWorkerForNavigationHintCallback& callback);

  void DidFindRegistrationForNavigationHint(
      const StartServiceWorkerForNavigationHintCallback& callback,
      ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  void DidStartServiceWorkerForNavigationHint(
      const GURL& pattern,
      const StartServiceWorkerForNavigationHintCallback& callback,
      ServiceWorkerStatusCode code);

  void RecordStartServiceWorkerForNavigationHintResult(
      const StartServiceWorkerForNavigationHintCallback& callback,
      StartServiceWorkerForNavigationHintResult result);

  // The core context is only for use on the IO thread.
  // Can be null before/during init, during/after shutdown, and after
  // DeleteAndStartOver fails.
  ServiceWorkerContextCore* context();

  // Observers of |context_core_| which live within content's implementation
  // boundary. Shared with |context_core_|.
  const scoped_refptr<
      base::ObserverListThreadSafe<ServiceWorkerContextCoreObserver>>
      core_observer_list_;

  // Observers which live outside content's implementation boundary. Observer
  // methods will always be dispatched on the UI thread.
  base::ObserverList<ServiceWorkerContextObserver> observer_list_;

  const std::unique_ptr<ServiceWorkerProcessManager> process_manager_;
  // Cleared in ShutdownOnIO():
  std::unique_ptr<ServiceWorkerContextCore> context_core_;

  // Initialized in Init(); true if the user data directory is empty.
  bool is_incognito_;

  // Raw pointer to the StoragePartitionImpl owning |this|.
  StoragePartitionImpl* storage_partition_;

  // The ResourceContext associated with this context.
  ResourceContext* resource_context_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_
