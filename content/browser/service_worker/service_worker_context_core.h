// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/content_export.h"

class GURL;

namespace base {
class FilePath;
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace net {
class URLRequestContext;
}

namespace storage {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

namespace content {

class EmbeddedWorkerRegistry;
class ServiceWorkerCacheStorageManager;
class ServiceWorkerContextObserver;
class ServiceWorkerContextWrapper;
class ServiceWorkerDatabaseTaskManager;
class ServiceWorkerHandle;
class ServiceWorkerJobCoordinator;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
class ServiceWorkerStorage;

// This class manages data associated with service workers.
// The class is single threaded and should only be used on the IO thread.
// In chromium, there is one instance per storagepartition. This class
// is the root of the containment hierarchy for service worker data
// associated with a particular partition.
class CONTENT_EXPORT ServiceWorkerContextCore
    : NON_EXPORTED_BASE(public ServiceWorkerVersion::Listener) {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode status)> StatusCallback;
  typedef base::Callback<void(ServiceWorkerStatusCode status,
                              const std::string& status_message,
                              int64 registration_id)> RegistrationCallback;
  typedef base::Callback<
      void(ServiceWorkerStatusCode status)> UnregistrationCallback;
  typedef IDMap<ServiceWorkerProviderHost, IDMapOwnPointer> ProviderMap;
  typedef IDMap<ProviderMap, IDMapOwnPointer> ProcessToProviderMap;

  // Directory for ServiceWorkerStorage and ServiceWorkerCacheManager.
  static const base::FilePath::CharType kServiceWorkerDirectory[];

  // Iterates over ServiceWorkerProviderHost objects in a ProcessToProviderMap.
  class ProviderHostIterator {
   public:
    ~ProviderHostIterator();
    ServiceWorkerProviderHost* GetProviderHost();
    void Advance();
    bool IsAtEnd();

   private:
    friend class ServiceWorkerContextCore;
    explicit ProviderHostIterator(ProcessToProviderMap* map);
    void Initialize();

    ProcessToProviderMap* map_;
    scoped_ptr<ProcessToProviderMap::iterator> process_iterator_;
    scoped_ptr<ProviderMap::iterator> provider_host_iterator_;

    DISALLOW_COPY_AND_ASSIGN(ProviderHostIterator);
  };

  // This is owned by the StoragePartition, which will supply it with
  // the local path on disk. Given an empty |user_data_directory|,
  // nothing will be stored on disk. |observer_list| is created in
  // ServiceWorkerContextWrapper. When Notify() of |observer_list| is called in
  // ServiceWorkerContextCore, the methods of ServiceWorkerContextObserver will
  // be called on the thread which called AddObserver() of |observer_list|.
  ServiceWorkerContextCore(
      const base::FilePath& user_data_directory,
      const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner,
      scoped_ptr<ServiceWorkerDatabaseTaskManager> database_task_runner_manager,
      const scoped_refptr<base::SingleThreadTaskRunner>& disk_cache_thread,
      storage::QuotaManagerProxy* quota_manager_proxy,
      storage::SpecialStoragePolicy* special_storage_policy,
      ObserverListThreadSafe<ServiceWorkerContextObserver>* observer_list,
      ServiceWorkerContextWrapper* wrapper);
  ServiceWorkerContextCore(
      ServiceWorkerContextCore* old_context,
      ServiceWorkerContextWrapper* wrapper);
  ~ServiceWorkerContextCore() override;

  // ServiceWorkerVersion::Listener overrides.
  void OnRunningStateChanged(ServiceWorkerVersion* version) override;
  void OnVersionStateChanged(ServiceWorkerVersion* version) override;
  void OnErrorReported(ServiceWorkerVersion* version,
                       const base::string16& error_message,
                       int line_number,
                       int column_number,
                       const GURL& source_url) override;
  void OnReportConsoleMessage(ServiceWorkerVersion* version,
                              int source_identifier,
                              int message_level,
                              const base::string16& message,
                              int line_number,
                              const GURL& source_url) override;

  ServiceWorkerContextWrapper* wrapper() const { return wrapper_; }
  ServiceWorkerStorage* storage() { return storage_.get(); }
  ServiceWorkerCacheStorageManager* cache_manager() {
    return cache_manager_.get();
  }
  ServiceWorkerProcessManager* process_manager();
  EmbeddedWorkerRegistry* embedded_worker_registry() {
    return embedded_worker_registry_.get();
  }
  ServiceWorkerJobCoordinator* job_coordinator() {
    return job_coordinator_.get();
  }

  // The context class owns the set of ProviderHosts.
  ServiceWorkerProviderHost* GetProviderHost(int process_id, int provider_id);
  void AddProviderHost(scoped_ptr<ServiceWorkerProviderHost> provider_host);
  void RemoveProviderHost(int process_id, int provider_id);
  void RemoveAllProviderHostsForProcess(int process_id);
  scoped_ptr<ProviderHostIterator> GetProviderHostIterator();

  // A child process of |source_process_id| may be used to run the created
  // worker for initial installation.
  // Non-null |provider_host| must be given if this is called from a document.
  void RegisterServiceWorker(const GURL& pattern,
                             const GURL& script_url,
                             ServiceWorkerProviderHost* provider_host,
                             const RegistrationCallback& callback);
  void UnregisterServiceWorker(const GURL& pattern,
                               const UnregistrationCallback& callback);
  // Callback is called issued after all unregistrations occur.  The Status
  // is populated as SERVICE_WORKER_OK if all succeed, or SERVICE_WORKER_FAILED
  // if any did not succeed.
  void UnregisterServiceWorkers(const GURL& origin,
                                const UnregistrationCallback& callback);
  void UpdateServiceWorker(ServiceWorkerRegistration* registration);

  // This class maintains collections of live instances, this class
  // does not own these object or influence their lifetime.
  ServiceWorkerRegistration* GetLiveRegistration(int64 registration_id);
  void AddLiveRegistration(ServiceWorkerRegistration* registration);
  void RemoveLiveRegistration(int64 registration_id);
  ServiceWorkerVersion* GetLiveVersion(int64 version_id);
  void AddLiveVersion(ServiceWorkerVersion* version);
  void RemoveLiveVersion(int64 registration_id);

  std::vector<ServiceWorkerRegistrationInfo> GetAllLiveRegistrationInfo();
  std::vector<ServiceWorkerVersionInfo> GetAllLiveVersionInfo();

  // Returns new context-local unique ID.
  int GetNewServiceWorkerHandleId();
  int GetNewRegistrationHandleId();

  void ScheduleDeleteAndStartOver() const;

  // Deletes all files on disk and restarts the system. This leaves the system
  // in a disabled state until it's done.
  void DeleteAndStartOver(const StatusCallback& callback);

  void SetBlobParametersForCache(
      net::URLRequestContext* request_context,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context);

  // Methods to support cross site navigations.
  scoped_ptr<ServiceWorkerProviderHost> TransferProviderHostOut(
      int process_id,
      int provider_id);
  void TransferProviderHostIn(
      int new_process_id,
      int new_host_id,
      scoped_ptr<ServiceWorkerProviderHost> provider_host);

  base::WeakPtr<ServiceWorkerContextCore> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend class ServiceWorkerContext;
  typedef std::map<int64, ServiceWorkerRegistration*> RegistrationsMap;
  typedef std::map<int64, ServiceWorkerVersion*> VersionMap;

  ProviderMap* GetProviderMapForProcess(int process_id) {
    return providers_->Lookup(process_id);
  }

  void RegistrationComplete(const GURL& pattern,
                            const RegistrationCallback& callback,
                            ServiceWorkerStatusCode status,
                            const std::string& status_message,
                            ServiceWorkerRegistration* registration);

  void UnregistrationComplete(const GURL& pattern,
                              const UnregistrationCallback& callback,
                              int64 registration_id,
                              ServiceWorkerStatusCode status);

  void DidGetAllRegistrationsForUnregisterForOrigin(
      const UnregistrationCallback& result,
      const GURL& origin,
      const std::vector<ServiceWorkerRegistrationInfo>& registrations);

  // It's safe to store a raw pointer instead of a scoped_refptr to |wrapper_|
  // because the Wrapper::Shutdown call that hops threads to destroy |this| uses
  // Bind() to hold a reference to |wrapper_| until |this| is fully destroyed.
  ServiceWorkerContextWrapper* wrapper_;
  scoped_ptr<ProcessToProviderMap> providers_;
  scoped_ptr<ServiceWorkerStorage> storage_;
  scoped_ptr<ServiceWorkerCacheStorageManager> cache_manager_;
  scoped_refptr<EmbeddedWorkerRegistry> embedded_worker_registry_;
  scoped_ptr<ServiceWorkerJobCoordinator> job_coordinator_;
  std::map<int64, ServiceWorkerRegistration*> live_registrations_;
  std::map<int64, ServiceWorkerVersion*> live_versions_;
  int next_handle_id_;
  int next_registration_handle_id_;
  scoped_refptr<ObserverListThreadSafe<ServiceWorkerContextObserver> >
      observer_list_;
  base::WeakPtrFactory<ServiceWorkerContextCore> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextCore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_
