// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WATCHER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WATCHER_H_

#include <vector>

#include "base/callback.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_info.h"

namespace content {

class ServiceWorkerContextWrapper;

// Used to monitor the status change of the ServiceWorker registrations and
// versions in the ServiceWorkerContext from UI thread.
class ServiceWorkerContextWatcher
    : public ServiceWorkerContextObserver,
      public base::RefCountedThreadSafe<ServiceWorkerContextWatcher> {
 public:
  typedef base::Callback<void(
      const std::vector<ServiceWorkerRegistrationInfo>&)>
      WorkerRegistrationUpdatedCallback;
  typedef base::Callback<void(const std::vector<ServiceWorkerVersionInfo>&)>
      WorkerVersionUpdatedCallback;
  ServiceWorkerContextWatcher(
      scoped_refptr<ServiceWorkerContextWrapper> context,
      const WorkerRegistrationUpdatedCallback& registration_callback,
      const WorkerVersionUpdatedCallback& version_callback);
  void Start();
  void Stop();

 private:
  friend class base::RefCountedThreadSafe<ServiceWorkerContextWatcher>;
  ~ServiceWorkerContextWatcher() override;

  void GetStoredRegistrationsOnIOThread();
  void OnStoredRegistrationsOnIOThread(
      const std::vector<ServiceWorkerRegistrationInfo>& stored_registrations);
  void StopOnIOThread();

  void StoreRegistrationInfo(
      const ServiceWorkerRegistrationInfo& registration,
      base::ScopedPtrHashMap<int64, ServiceWorkerRegistrationInfo>* info_map);
  void StoreVersionInfo(const ServiceWorkerVersionInfo& version);

  void SendRegistrationInfo(
      int64 registration_id,
      const GURL& pattern,
      ServiceWorkerRegistrationInfo::DeleteFlag delete_flag);
  void SendVersionInfo(const ServiceWorkerVersionInfo& version);

  // ServiceWorkerContextObserver implements
  void OnNewLiveRegistration(int64 registration_id,
                             const GURL& pattern) override;
  void OnNewLiveVersion(int64 version_id,
                        int64 registration_id,
                        const GURL& script_url) override;
  void OnRunningStateChanged(
      int64 version_id,
      content::ServiceWorkerVersion::RunningStatus running_status) override;
  void OnVersionStateChanged(
      int64 version_id,
      content::ServiceWorkerVersion::Status status) override;
  void OnRegistrationStored(int64 registration_id,
                            const GURL& pattern) override;
  void OnRegistrationDeleted(int64 registration_id,
                             const GURL& pattern) override;

  base::ScopedPtrHashMap<int64, ServiceWorkerVersionInfo> version_info_map_;
  scoped_refptr<ServiceWorkerContextWrapper> context_;
  WorkerRegistrationUpdatedCallback registration_callback_;
  WorkerVersionUpdatedCallback version_callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WATCHER_H_
