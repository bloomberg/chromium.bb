// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_

#include <map>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_database.h"
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
class ServiceWorkerVersion;

// This class provides an interface to store and retrieve ServiceWorker
// registration data.
class CONTENT_EXPORT ServiceWorkerStorage {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode status)> StatusCallback;
  typedef base::Callback<void(ServiceWorkerStatusCode status,
                              const scoped_refptr<ServiceWorkerRegistration>&
                                  registration)> FindRegistrationCallback;
  typedef base::Callback<
      void(const std::vector<ServiceWorkerRegistrationInfo>& registrations)>
          GetAllRegistrationInfosCallback;
  typedef base::Callback<
      void(ServiceWorkerStatusCode status, int result)>
          CompareCallback;

  struct InitialData {
    int64 next_registration_id;
    int64 next_version_id;
    int64 next_resource_id;
    std::set<GURL> origins;

    InitialData();
    ~InitialData();
  };

  ServiceWorkerStorage(const base::FilePath& path,
                       base::WeakPtr<ServiceWorkerContextCore> context,
                       base::SequencedTaskRunner* database_task_runner,
                       base::MessageLoopProxy* disk_cache_thread,
                       quota::QuotaManagerProxy* quota_manager_proxy);
  ~ServiceWorkerStorage();

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
  // A pre-existing version's script resources will remain available until
  // either a browser restart or DeleteVersionResources is called.
  void StoreRegistration(
      ServiceWorkerRegistration* registration,
      ServiceWorkerVersion* version,
      const StatusCallback& callback);

  // Updates the state of the registration's stored version to active.
  void UpdateToActiveState(
      ServiceWorkerRegistration* registration,
      const StatusCallback& callback);

  // Deletes the registration data for |registration_id|, the
  // script resources for the registration's stored version
  // will remain available until either a browser restart or
  // DeleteVersionResources is called.
  void DeleteRegistration(int64 registration_id,
                          const GURL& origin,
                          const StatusCallback& callback);

  scoped_ptr<ServiceWorkerResponseReader> CreateResponseReader(
      int64 response_id);
  scoped_ptr<ServiceWorkerResponseWriter> CreateResponseWriter(
      int64 response_id);

  // Returns new IDs which are guaranteed to be unique in the storage.
  int64 NewRegistrationId();
  int64 NewVersionId();
  int64 NewResourceId();

  // Intended for use only by ServiceWorkerRegisterJob.
  void NotifyInstallingRegistration(
      ServiceWorkerRegistration* registration);
  void NotifyDoneInstallingRegistration(
      ServiceWorkerRegistration* registration);

 private:
  friend class ServiceWorkerStorageTest;

  typedef std::vector<ServiceWorkerDatabase::RegistrationData> RegistrationList;
  typedef std::vector<ServiceWorkerDatabase::ResourceRecord> ResourceList;

  bool LazyInitialize(
      const base::Closure& callback);
  void DidReadInitialData(
      InitialData* data,
      ServiceWorkerDatabase::Status status);
  void DidGetRegistrationsForPattern(
      const GURL& scope,
      const FindRegistrationCallback& callback,
      RegistrationList* registrations,
      ServiceWorkerDatabase::Status status);
  void DidGetRegistrationsForDocument(
      const GURL& scope,
      const FindRegistrationCallback& callback,
      RegistrationList* registrations,
      ServiceWorkerDatabase::Status status);
  void DidReadRegistrationForId(
      const FindRegistrationCallback& callback,
      const ServiceWorkerDatabase::RegistrationData& registration,
      const ResourceList& resources,
      ServiceWorkerDatabase::Status status);
  void DidGetAllRegistrations(
      const GetAllRegistrationInfosCallback& callback,
      RegistrationList* registrations,
      ServiceWorkerDatabase::Status status);
  void DidStoreRegistration(
      const GURL& origin,
      const StatusCallback& callback,
      bool success);
  void DidDeleteRegistration(
      const GURL& origin,
      const StatusCallback& callback,
      bool origin_is_deletable,
      ServiceWorkerDatabase::Status status);

  scoped_refptr<ServiceWorkerRegistration> CreateRegistration(
      const ServiceWorkerDatabase::RegistrationData& data);
  ServiceWorkerRegistration* FindInstallingRegistrationForDocument(
      const GURL& document_url);
  ServiceWorkerRegistration* FindInstallingRegistrationForPattern(
      const GURL& scope);
  ServiceWorkerRegistration* FindInstallingRegistrationForId(
      int64 registration_id);

  // For finding registrations being installed.
  typedef std::map<int64, scoped_refptr<ServiceWorkerRegistration> >
      RegistrationRefsById;
  RegistrationRefsById installing_registrations_;

  // Lazy disk_cache getter.
  ServiceWorkerDiskCache* disk_cache();

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

  base::WeakPtrFactory<ServiceWorkerStorage> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerStorage);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_
