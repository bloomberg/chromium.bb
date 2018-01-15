// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/id_map.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/content_export.h"
#include "content/public/browser/service_worker_context.h"
#include "third_party/WebKit/common/service_worker/service_worker_registration.mojom.h"

class GURL;

namespace base {
class FilePath;
}

namespace storage {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

namespace content {

class EmbeddedWorkerRegistry;
class ServiceWorkerContextCoreObserver;
class ServiceWorkerContextWrapper;
class ServiceWorkerDispatcherHost;
class ServiceWorkerJobCoordinator;
class ServiceWorkerNavigationHandleCore;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
class ServiceWorkerStorage;
class URLLoaderFactoryGetter;

// This class manages data associated with service workers.
// The class is single threaded and should only be used on the IO thread.
// In chromium, there is one instance per storagepartition. This class
// is the root of the containment hierarchy for service worker data
// associated with a particular partition.
class CONTENT_EXPORT ServiceWorkerContextCore
    : public ServiceWorkerVersion::Listener {
 public:
  using BoolCallback = base::Callback<void(bool)>;
  using StatusCallback = base::Callback<void(ServiceWorkerStatusCode status)>;
  using RegistrationCallback =
      base::Callback<void(ServiceWorkerStatusCode status,
                          const std::string& status_message,
                          int64_t registration_id)>;
  using UpdateCallback = base::Callback<void(ServiceWorkerStatusCode status,
                                             const std::string& status_message,
                                             int64_t registration_id)>;
  using UnregistrationCallback =
      base::Callback<void(ServiceWorkerStatusCode status)>;
  using ProviderMap = base::IDMap<std::unique_ptr<ServiceWorkerProviderHost>>;
  using ProcessToProviderMap = base::IDMap<std::unique_ptr<ProviderMap>>;

  using ProviderByClientUUIDMap =
      std::map<std::string, ServiceWorkerProviderHost*>;

  // Directory for ServiceWorkerStorage and ServiceWorkerCacheManager.
  static const base::FilePath::CharType kServiceWorkerDirectory[];

  // Iterates over ServiceWorkerProviderHost objects in a ProcessToProviderMap.
  class CONTENT_EXPORT ProviderHostIterator {
   public:
    ~ProviderHostIterator();
    ServiceWorkerProviderHost* GetProviderHost();
    void Advance();
    bool IsAtEnd();

   private:
    friend class ServiceWorkerContextCore;
    using ProviderHostPredicate =
        base::Callback<bool(ServiceWorkerProviderHost*)>;
    ProviderHostIterator(ProcessToProviderMap* map,
                         const ProviderHostPredicate& predicate);
    void Initialize();
    bool ForwardUntilMatchingProviderHost();

    ProcessToProviderMap* map_;
    ProviderHostPredicate predicate_;
    std::unique_ptr<ProcessToProviderMap::iterator> process_iterator_;
    std::unique_ptr<ProviderMap::iterator> provider_host_iterator_;

    DISALLOW_COPY_AND_ASSIGN(ProviderHostIterator);
  };

  // This is owned by the StoragePartition, which will supply it with
  // the local path on disk. Given an empty |user_data_directory|,
  // nothing will be stored on disk. |observer_list| is created in
  // ServiceWorkerContextWrapper. When Notify() of |observer_list| is called in
  // ServiceWorkerContextCore, the methods of ServiceWorkerContextCoreObserver
  // will be called on the thread which called AddObserver() of |observer_list|.
  // |url_loader_factory_getter| is used only when IsServicificationEnabled is
  // true.
  ServiceWorkerContextCore(
      const base::FilePath& user_data_directory,
      scoped_refptr<base::SequencedTaskRunner> database_task_runner,
      storage::QuotaManagerProxy* quota_manager_proxy,
      storage::SpecialStoragePolicy* special_storage_policy,
      URLLoaderFactoryGetter* url_loader_factory_getter,
      base::ObserverListThreadSafe<ServiceWorkerContextCoreObserver>*
          observer_list,
      ServiceWorkerContextWrapper* wrapper);
  ServiceWorkerContextCore(
      ServiceWorkerContextCore* old_context,
      ServiceWorkerContextWrapper* wrapper);
  ~ServiceWorkerContextCore() override;

  void OnStorageWiped();

  // ServiceWorkerVersion::Listener overrides.
  void OnRunningStateChanged(ServiceWorkerVersion* version) override;
  void OnVersionStateChanged(ServiceWorkerVersion* version) override;
  void OnDevToolsRoutingIdChanged(ServiceWorkerVersion* version) override;
  void OnMainScriptHttpResponseInfoSet(ServiceWorkerVersion* version) override;
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
  void OnControlleeAdded(ServiceWorkerVersion* version,
                         ServiceWorkerProviderHost* provider_host) override;
  void OnControlleeRemoved(ServiceWorkerVersion* version,
                           ServiceWorkerProviderHost* provider_host) override;

  ServiceWorkerContextWrapper* wrapper() const { return wrapper_; }
  ServiceWorkerStorage* storage() { return storage_.get(); }
  ServiceWorkerProcessManager* process_manager();
  EmbeddedWorkerRegistry* embedded_worker_registry() {
    return embedded_worker_registry_.get();
  }
  ServiceWorkerJobCoordinator* job_coordinator() {
    return job_coordinator_.get();
  }

  // Maintains DispatcherHosts to exchange service worker related messages
  // through them. The DispatcherHosts are not owned by this class.
  void AddDispatcherHost(int process_id,
                         ServiceWorkerDispatcherHost* dispatcher_host);
  ServiceWorkerDispatcherHost* GetDispatcherHost(int process_id);
  void RemoveDispatcherHost(int process_id);

  // The context class owns the set of ProviderHosts.
  void AddProviderHost(
      std::unique_ptr<ServiceWorkerProviderHost> provider_host);
  ServiceWorkerProviderHost* GetProviderHost(int process_id, int provider_id);
  void RemoveProviderHost(int process_id, int provider_id);
  void RemoveAllProviderHostsForProcess(int process_id);
  std::unique_ptr<ProviderHostIterator> GetProviderHostIterator();

  // Returns a ProviderHost iterator for all ServiceWorker clients for
  // the |origin|.  This only returns ProviderHosts that are of CONTROLLEE
  // and belong to the |origin|.
  std::unique_ptr<ProviderHostIterator> GetClientProviderHostIterator(
      const GURL& origin);

  // Runs the callback with true if there is a ProviderHost for |origin| of type
  // blink::mojom::ServiceWorkerProviderType::kForWindow which is a main
  // (top-level) frame.
  void HasMainFrameProviderHost(const GURL& origin,
                                const BoolCallback& callback) const;

  // Maintains a map from Client UUID to ProviderHost.
  // (Note: instead of maintaining 2 maps we might be able to uniformly use
  // UUID instead of process_id+provider_id elsewhere. For now I'm leaving
  // these as provider_id is deeply wired everywhere)
  void RegisterProviderHostByClientID(const std::string& client_uuid,
                                      ServiceWorkerProviderHost* provider_host);
  void UnregisterProviderHostByClientID(const std::string& client_uuid);
  ServiceWorkerProviderHost* GetProviderHostByClientID(
      const std::string& client_uuid);

  void RegisterServiceWorker(
      const GURL& script_url,
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      const RegistrationCallback& callback);
  void UnregisterServiceWorker(const GURL& pattern,
                               const UnregistrationCallback& callback);

  // Callback is called after all deletions occured. The status code is
  // SERVICE_WORKER_OK if all succeed, or SERVICE_WORKER_FAILED
  // if any did not succeed.
  void DeleteForOrigin(
      const GURL& origin,
      base::OnceCallback<void(ServiceWorkerStatusCode)> callback);

  // Updates the service worker. If |force_bypass_cache| is true or 24 hours
  // have passed since the last update, bypasses the browser cache.
  void UpdateServiceWorker(ServiceWorkerRegistration* registration,
                           bool force_bypass_cache);
  void UpdateServiceWorker(ServiceWorkerRegistration* registration,
                           bool force_bypass_cache,
                           bool skip_script_comparison,
                           const UpdateCallback& callback);

  // Used in DevTools to update the service worker registrations without
  // consulting the browser cache while loading the controlled page. The
  // loading is delayed until the update completes and the new worker is
  // activated. The new worker skips the waiting state and immediately
  // becomes active after installed.
  bool force_update_on_page_load() { return force_update_on_page_load_; }
  void set_force_update_on_page_load(bool force_update_on_page_load) {
    force_update_on_page_load_ = force_update_on_page_load;
  }

  // This class maintains collections of live instances, this class
  // does not own these object or influence their lifetime.
  ServiceWorkerRegistration* GetLiveRegistration(int64_t registration_id);
  void AddLiveRegistration(ServiceWorkerRegistration* registration);
  void RemoveLiveRegistration(int64_t registration_id);
  const std::map<int64_t, ServiceWorkerRegistration*>& GetLiveRegistrations()
      const {
    return live_registrations_;
  }
  ServiceWorkerVersion* GetLiveVersion(int64_t version_id);
  void AddLiveVersion(ServiceWorkerVersion* version);
  void RemoveLiveVersion(int64_t registration_id);
  const std::map<int64_t, ServiceWorkerVersion*>& GetLiveVersions() const {
    return live_versions_;
  }

  // PlzNavigate
  // Methods to manage the map keeping track of all
  // ServiceWorkerNavigationHandleCores registered for ongoing navigations.
  void AddNavigationHandleCore(int service_worker_provider_id,
                               ServiceWorkerNavigationHandleCore* handle);
  void RemoveNavigationHandleCore(int service_worker_provider_id);
  ServiceWorkerNavigationHandleCore* GetNavigationHandleCore(
      int service_worker_provider_id);

  std::vector<ServiceWorkerRegistrationInfo> GetAllLiveRegistrationInfo();
  std::vector<ServiceWorkerVersionInfo> GetAllLiveVersionInfo();

  // ProtectVersion holds a reference to |version| until UnprotectVersion is
  // called.
  void ProtectVersion(const scoped_refptr<ServiceWorkerVersion>& version);
  void UnprotectVersion(int64_t version_id);

  // Returns new context-local unique ID.
  int GetNewServiceWorkerHandleId();

  void ScheduleDeleteAndStartOver() const;

  // Deletes all files on disk and restarts the system. This leaves the system
  // in a disabled state until it's done.
  void DeleteAndStartOver(const StatusCallback& callback);

  void ClearAllServiceWorkersForTest(base::OnceClosure callback);

  // Determines if there is a ServiceWorker registration that matches |url|, and
  // if |other_url| falls inside the scope of the same registration. See
  // ServiceWorkerContext::CheckHasServiceWorker for more details.
  void CheckHasServiceWorker(
      const GURL& url,
      const GURL& other_url,
      const ServiceWorkerContext::CheckHasServiceWorkerCallback callback);

  void UpdateVersionFailureCount(int64_t version_id,
                                 ServiceWorkerStatusCode status);
  // Returns the count of consecutive start worker failures for the given
  // version. The count resets to zero when the worker successfully starts.
  int GetVersionFailureCount(int64_t version_id);

  URLLoaderFactoryGetter* loader_factory_getter() {
    return loader_factory_getter_.get();
  }

  base::WeakPtr<ServiceWorkerContextCore> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend class ServiceWorkerContextCoreTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextCoreTest, FailureInfo);

  typedef std::map<int64_t, ServiceWorkerRegistration*> RegistrationsMap;
  typedef std::map<int64_t, ServiceWorkerVersion*> VersionMap;

  struct FailureInfo {
    int count;
    ServiceWorkerStatusCode last_failure;
  };

  ProviderMap* GetProviderMapForProcess(int process_id);

  void RegistrationComplete(const GURL& pattern,
                            const RegistrationCallback& callback,
                            ServiceWorkerStatusCode status,
                            const std::string& status_message,
                            ServiceWorkerRegistration* registration);

  void UpdateComplete(const UpdateCallback& callback,
                      ServiceWorkerStatusCode status,
                      const std::string& status_message,
                      ServiceWorkerRegistration* registration);

  void UnregistrationComplete(const GURL& pattern,
                              const UnregistrationCallback& callback,
                              int64_t registration_id,
                              ServiceWorkerStatusCode status);

  void DidGetRegistrationsForDeleteForOrigin(
      base::OnceCallback<void(ServiceWorkerStatusCode)> callback,
      ServiceWorkerStatusCode status,
      const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
          registrations);

  void DidFindRegistrationForCheckHasServiceWorker(
      const GURL& other_url,
      const ServiceWorkerContext::CheckHasServiceWorkerCallback callback,
      ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void OnRegistrationFinishedForCheckHasServiceWorker(
      const ServiceWorkerContext::CheckHasServiceWorkerCallback callback,
      scoped_refptr<ServiceWorkerRegistration> registration);

  // It's safe to store a raw pointer instead of a scoped_refptr to |wrapper_|
  // because the Wrapper::Shutdown call that hops threads to destroy |this| uses
  // Bind() to hold a reference to |wrapper_| until |this| is fully destroyed.
  ServiceWorkerContextWrapper* wrapper_;
  std::map<int /* process_id */, ServiceWorkerDispatcherHost*>
      dispatcher_hosts_;
  std::unique_ptr<ProcessToProviderMap> providers_;
  std::unique_ptr<ProviderByClientUUIDMap> provider_by_uuid_;
  std::unique_ptr<ServiceWorkerStorage> storage_;
  scoped_refptr<EmbeddedWorkerRegistry> embedded_worker_registry_;
  std::unique_ptr<ServiceWorkerJobCoordinator> job_coordinator_;
  std::map<int64_t, ServiceWorkerRegistration*> live_registrations_;
  std::map<int64_t, ServiceWorkerVersion*> live_versions_;
  std::map<int64_t, scoped_refptr<ServiceWorkerVersion>> protected_versions_;

  std::map<int64_t /* version_id */, FailureInfo> failure_counts_;

  // PlzNavigate
  // Map of ServiceWorkerNavigationHandleCores used for navigation requests.
  std::map<int, ServiceWorkerNavigationHandleCore*>
      navigation_handle_cores_map_;

  // IsServicificationEnabled
  scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter_;

  bool force_update_on_page_load_;
  int next_handle_id_;
  // Set in RegisterServiceWorker(), cleared in ClearAllServiceWorkersForTest().
  // This is used to avoid unnecessary disk read operation in tests. This value
  // is false if Chrome was relaunched after service workers were registered.
  bool was_service_worker_registered_;
  scoped_refptr<base::ObserverListThreadSafe<ServiceWorkerContextCoreObserver>>
      observer_list_;
  base::WeakPtrFactory<ServiceWorkerContextCore> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextCore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_
