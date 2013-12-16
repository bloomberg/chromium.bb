// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_

#include <map>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/content_export.h"

class GURL;

namespace base {
class FilePath;
}

namespace quota {
class QuotaManagerProxy;
}

namespace content {

class EmbeddedWorkerRegistry;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
class ServiceWorkerStorage;

// This class manages data associated with service workers.
// The class is single threaded and should only be used on the IO thread.
// In chromium, there is one instance per storagepartition. This class
// is the root of the containment hierarchy for service worker data
// associated with a particular partition.
class CONTENT_EXPORT ServiceWorkerContextCore
    : NON_EXPORTED_BASE(
          public base::SupportsWeakPtr<ServiceWorkerContextCore>) {
 public:
  typedef base::Callback<void(ServiceWorkerRegistrationStatus status,
                              int64 registration_id)> RegistrationCallback;
  typedef base::Callback<
      void(ServiceWorkerRegistrationStatus status)> UnregistrationCallback;

  // This is owned by the StoragePartition, which will supply it with
  // the local path on disk. Given an empty |user_data_directory|,
  // nothing will be stored on disk.
  ServiceWorkerContextCore(const base::FilePath& user_data_directory,
                           quota::QuotaManagerProxy* quota_manager_proxy);
  ~ServiceWorkerContextCore();

  ServiceWorkerStorage* storage() { return storage_.get(); }

  // The context class owns the set of ProviderHosts.
  ServiceWorkerProviderHost* GetProviderHost(int process_id, int provider_id);
  void AddProviderHost(scoped_ptr<ServiceWorkerProviderHost> provider_host);
  void RemoveProviderHost(int process_id, int provider_id);
  void RemoveAllProviderHostsForProcess(int process_id);

  // Checks the cmdline flag.
  bool IsEnabled();

  // The callback will be called on the IO thread.
  void RegisterServiceWorker(const GURL& pattern,
                             const GURL& script_url,
                             const RegistrationCallback& callback);

  // The callback will be called on the IO thread.
  void UnregisterServiceWorker(const GURL& pattern,
                               const UnregistrationCallback& callback);

  EmbeddedWorkerRegistry* embedded_worker_registry() {
    return embedded_worker_registry_.get();
  }

 private:
  typedef IDMap<ServiceWorkerProviderHost, IDMapOwnPointer> ProviderMap;
  typedef IDMap<ProviderMap, IDMapOwnPointer> ProcessToProviderMap;

  ProviderMap* GetProviderMapForProcess(int process_id) {
    return providers_.Lookup(process_id);
  }

  void RegistrationComplete(
      const RegistrationCallback& callback,
      ServiceWorkerRegistrationStatus status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);

  ProcessToProviderMap providers_;
  scoped_ptr<ServiceWorkerStorage> storage_;
  scoped_refptr<EmbeddedWorkerRegistry> embedded_worker_registry_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextCore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_
