// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_

#include <deque>
#include <map>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_database.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "url/gurl.h"

namespace base {
class MessageLoopProxy;
class SequencedTaskRunner;
}

namespace quota {
class QuotaManagerProxy;
}

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerDiskCache;
class ServiceWorkerRegistration;
class ServiceWorkerRegistrationInfo;
class ServiceWorkerResponseReader;
class ServiceWorkerResponseWriter;

// This class provides an interface to store and retrieve ServiceWorker
// registration data.
class CONTENT_EXPORT ServiceWorkerStorage
    : NON_EXPORTED_BASE(public ServiceWorkerVersion::Listener) {
 public:
  typedef std::vector<ServiceWorkerDatabase::ResourceRecord> ResourceList;
  typedef base::Callback<void(ServiceWorkerStatusCode status)> StatusCallback;
  typedef base::Callback<void(ServiceWorkerStatusCode status,
                              const scoped_refptr<ServiceWorkerRegistration>&
                                  registration)> FindRegistrationCallback;
  typedef base::Callback<
      void(const std::vector<ServiceWorkerRegistrationInfo>& registrations)>
          GetAllRegistrationInfosCallback;
  typedef base::Callback<
      void(ServiceWorkerStatusCode status, bool are_equal)>
          CompareCallback;

  virtual ~ServiceWorkerStorage();

  static scoped_ptr<ServiceWorkerStorage> Create(
      const base::FilePath& path,
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::SequencedTaskRunner* database_task_runner,
      base::MessageLoopProxy* disk_cache_thread,
      quota::QuotaManagerProxy* quota_manager_proxy);

  // Used for DeleteAndStartOver. Creates new storage based on |old_storage|.
  static scoped_ptr<ServiceWorkerStorage> Create(
      base::WeakPtr<ServiceWorkerContextCore> context,
      ServiceWorkerStorage* old_storage);

  // Finds registration for |document_url| or |pattern| or |registration_id|.
  // The Find methods will find stored and initially installing registrations.
  // Returns SERVICE_WORKER_OK with non-null registration if registration
  // is found, or returns SERVICE_WORKER_ERROR_NOT_FOUND if no matching
  // registration is found.  The FindRegistrationForPattern method is
  // guaranteed to return asynchronously. However, the methods to find
  // for |document_url| or |registration_id| may complete immediately
  // (the callback may be called prior to the method returning) or
  // asynchronously.
  void FindRegistrationForDocument(const GURL& document_url,
                                   const FindRegistrationCallback& callback);
  void FindRegistrationForPattern(const GURL& scope,
                                  const FindRegistrationCallback& callback);
  void FindRegistrationForId(int64 registration_id,
                             const GURL& origin,
                             const FindRegistrationCallback& callback);

  // Returns info about all stored and initially installing registrations.
  void GetAllRegistrations(const GetAllRegistrationInfosCallback& callback);

  // Commits |registration| with the installed but not activated |version|
  // to storage, overwritting any pre-existing registration data for the scope.
  // A pre-existing version's script resources remain available if that version
  // is live. PurgeResources should be called when it's OK to delete them.
  void StoreRegistration(
      ServiceWorkerRegistration* registration,
      ServiceWorkerVersion* version,
      const StatusCallback& callback);

  // Updates the state of the registration's stored version to active.
  void UpdateToActiveState(
      ServiceWorkerRegistration* registration,
      const StatusCallback& callback);

  // Deletes the registration data for |registration_id|. If the registration's
  // version is live, its script resources will remain available.
  // PurgeResources should be called when it's OK to delete them.
  void DeleteRegistration(int64 registration_id,
                          const GURL& origin,
                          const StatusCallback& callback);

  scoped_ptr<ServiceWorkerResponseReader> CreateResponseReader(
      int64 response_id);
  scoped_ptr<ServiceWorkerResponseWriter> CreateResponseWriter(
      int64 response_id);

  // Adds |id| to the set of resources ids that are in the disk
  // cache but not yet stored with a registration.
  void StoreUncommittedResponseId(int64 id);

  // Removes |id| from uncommitted list, adds it to the
  // purgeable list and purges it.
  void DoomUncommittedResponse(int64 id);

  // Compares only the response bodies.
  void CompareScriptResources(int64 lhs_id, int64 rhs_id,
                              const CompareCallback& callback);

  // Deletes the storage and starts over.
  void DeleteAndStartOver(const StatusCallback& callback);

  // Returns new IDs which are guaranteed to be unique in the storage.
  int64 NewRegistrationId();
  int64 NewVersionId();
  int64 NewResourceId();

  // Intended for use only by ServiceWorkerRegisterJob.
  void NotifyInstallingRegistration(
      ServiceWorkerRegistration* registration);
  void NotifyDoneInstallingRegistration(
      ServiceWorkerRegistration* registration,
      ServiceWorkerVersion* version,
      ServiceWorkerStatusCode status);

  void Disable();
  bool IsDisabled() const;

  // |resources| must already be on the purgeable list.
  void PurgeResources(const ResourceList& resources);

 private:
  friend class ServiceWorkerResourceStorageTest;
  friend class ServiceWorkerControlleeRequestHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerResourceStorageTest,
                           DeleteRegistration_NoLiveVersion);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerResourceStorageTest,
                           DeleteRegistration_WaitingVersion);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerResourceStorageTest,
                           DeleteRegistration_ActiveVersion);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerResourceStorageTest,
                           UpdateRegistration);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerResourceStorageDiskTest,
                           CleanupOnRestart);

  struct InitialData {
    int64 next_registration_id;
    int64 next_version_id;
    int64 next_resource_id;
    std::set<GURL> origins;

    InitialData();
    ~InitialData();
  };

  // Because there are too many params for base::Bind to wrap a closure around.
  struct DidDeleteRegistrationParams {
    int64 registration_id;
    GURL origin;
    StatusCallback callback;

    DidDeleteRegistrationParams();
    ~DidDeleteRegistrationParams();
  };

  typedef std::vector<ServiceWorkerDatabase::RegistrationData> RegistrationList;
  typedef std::map<int64, scoped_refptr<ServiceWorkerRegistration> >
      RegistrationRefsById;
  typedef base::Callback<void(
      InitialData* data,
      ServiceWorkerDatabase::Status status)> InitializeCallback;
  typedef base::Callback<
      void(const GURL& origin,
           int64 deleted_version_id,
           const std::vector<int64>& newly_purgeable_resources,
           ServiceWorkerDatabase::Status status)> WriteRegistrationCallback;
  typedef base::Callback<
      void(bool origin_is_deletable,
           int64 version_id,
           const std::vector<int64>& newly_purgeable_resources,
           ServiceWorkerDatabase::Status status)> DeleteRegistrationCallback;
  typedef base::Callback<void(
      const ServiceWorkerDatabase::RegistrationData& data,
      const ResourceList& resources,
      ServiceWorkerDatabase::Status status)> FindInDBCallback;
  typedef base::Callback<void(const std::vector<int64>& resource_ids,
                              ServiceWorkerDatabase::Status status)>
      GetResourcesCallback;

  ServiceWorkerStorage(const base::FilePath& path,
                       base::WeakPtr<ServiceWorkerContextCore> context,
                       base::SequencedTaskRunner* database_task_runner,
                       base::MessageLoopProxy* disk_cache_thread,
                       quota::QuotaManagerProxy* quota_manager_proxy);

  base::FilePath GetDatabasePath();
  base::FilePath GetDiskCachePath();

  bool LazyInitialize(
      const base::Closure& callback);
  void DidReadInitialData(
      InitialData* data,
      ServiceWorkerDatabase::Status status);
  void DidFindRegistrationForDocument(
      const GURL& document_url,
      const FindRegistrationCallback& callback,
      const ServiceWorkerDatabase::RegistrationData& data,
      const ResourceList& resources,
      ServiceWorkerDatabase::Status status);
  void DidFindRegistrationForPattern(
      const GURL& scope,
      const FindRegistrationCallback& callback,
      const ServiceWorkerDatabase::RegistrationData& data,
      const ResourceList& resources,
      ServiceWorkerDatabase::Status status);
  void DidFindRegistrationForId(
      const FindRegistrationCallback& callback,
      const ServiceWorkerDatabase::RegistrationData& data,
      const ResourceList& resources,
      ServiceWorkerDatabase::Status status);
  void DidGetAllRegistrations(
      const GetAllRegistrationInfosCallback& callback,
      RegistrationList* registrations,
      ServiceWorkerDatabase::Status status);
  void DidStoreRegistration(const StatusCallback& callback,
                            const GURL& origin,
                            int64 deleted_version_id,
                            const std::vector<int64>& newly_purgeable_resources,
                            ServiceWorkerDatabase::Status status);
  void DidUpdateToActiveState(
      const StatusCallback& callback,
      ServiceWorkerDatabase::Status status);
  void DidDeleteRegistration(
      const DidDeleteRegistrationParams& params,
      bool origin_is_deletable,
      int64 version_id,
      const std::vector<int64>& newly_purgeable_resources,
      ServiceWorkerDatabase::Status status);
  void ReturnFoundRegistration(
      const FindRegistrationCallback& callback,
      const ServiceWorkerDatabase::RegistrationData& data,
      const ResourceList& resources);

  scoped_refptr<ServiceWorkerRegistration> GetOrCreateRegistration(
      const ServiceWorkerDatabase::RegistrationData& data,
      const ResourceList& resources);
  ServiceWorkerRegistration* FindInstallingRegistrationForDocument(
      const GURL& document_url);
  ServiceWorkerRegistration* FindInstallingRegistrationForPattern(
      const GURL& scope);
  ServiceWorkerRegistration* FindInstallingRegistrationForId(
      int64 registration_id);

  // Lazy disk_cache getter.
  ServiceWorkerDiskCache* disk_cache();
  void OnDiskCacheInitialized(int rv);

  void StartPurgingResources(const std::vector<int64>& ids);
  void StartPurgingResources(const ResourceList& resources);
  void ContinuePurgingResources();
  void PurgeResource(int64 id);
  void OnResourcePurged(int64 id, int rv);

  // Deletes purgeable and uncommitted resources left over from the previous
  // browser session. This must be called once per session before any database
  // operation that may mutate the purgeable or uncommitted resource lists.
  void DeleteStaleResources();
  void DidCollectStaleResources(const std::vector<int64>& stale_resource_ids,
                                ServiceWorkerDatabase::Status status);

  // Static cross-thread helpers.
  static void CollectStaleResourcesFromDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      const GetResourcesCallback& callback);
  static void ReadInitialDataFromDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      const InitializeCallback& callback);
  static void DeleteRegistrationFromDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      int64 registration_id,
      const GURL& origin,
      const DeleteRegistrationCallback& callback);
  static void WriteRegistrationInDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      const ServiceWorkerDatabase::RegistrationData& registration,
      const ResourceList& resources,
      const WriteRegistrationCallback& callback);
  static void FindForDocumentInDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      const GURL& document_url,
      const FindInDBCallback& callback);
  static void FindForPatternInDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      const GURL& scope,
      const FindInDBCallback& callback);
  static void FindForIdInDB(
      ServiceWorkerDatabase* database,
      scoped_refptr<base::SequencedTaskRunner> original_task_runner,
      int64 registration_id,
      const GURL& origin,
      const FindInDBCallback& callback);

  void ScheduleDeleteAndStartOver();
  void DidDeleteDatabase(
      const StatusCallback& callback,
      ServiceWorkerDatabase::Status status);
  void DidDeleteDiskCache(
      const StatusCallback& callback,
      bool result);

  // For finding registrations being installed.
  RegistrationRefsById installing_registrations_;

  // Origins having registations.
  std::set<GURL> registered_origins_;

  // Pending database tasks waiting for initialization.
  std::vector<base::Closure> pending_tasks_;

  int64 next_registration_id_;
  int64 next_version_id_;
  int64 next_resource_id_;

  enum State {
    UNINITIALIZED,
    INITIALIZING,
    INITIALIZED,
    DISABLED,
  };
  State state_;

  base::FilePath path_;
  base::WeakPtr<ServiceWorkerContextCore> context_;

  // Only accessed on |database_task_runner_|.
  scoped_ptr<ServiceWorkerDatabase> database_;

  scoped_refptr<base::SequencedTaskRunner> database_task_runner_;
  scoped_refptr<base::MessageLoopProxy> disk_cache_thread_;
  scoped_refptr<quota::QuotaManagerProxy> quota_manager_proxy_;
  scoped_ptr<ServiceWorkerDiskCache> disk_cache_;
  std::deque<int64> purgeable_resource_ids_;
  bool is_purge_pending_;
  bool has_checked_for_stale_resources_;
  std::set<int64> pending_deletions_;

  base::WeakPtrFactory<ServiceWorkerStorage> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerStorage);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_
