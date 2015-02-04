// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerRegistrationInfo;
class ServiceWorkerVersion;

// This class represents a Service Worker registration. The scope is constant
// for the life of the persistent registration. It's refcounted to facilitate
// multiple controllees being associated with the same registration.
class CONTENT_EXPORT ServiceWorkerRegistration
    : NON_EXPORTED_BASE(public base::RefCounted<ServiceWorkerRegistration>),
      public ServiceWorkerVersion::Listener {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode status)> StatusCallback;
  typedef base::Callback<void(
      const std::string& data,
      ServiceWorkerStatusCode status)> GetUserDataCallback;

  class Listener {
   public:
    virtual void OnVersionAttributesChanged(
        ServiceWorkerRegistration* registration,
        ChangedVersionAttributesMask changed_mask,
        const ServiceWorkerRegistrationInfo& info) {}
    virtual void OnRegistrationFailed(
        ServiceWorkerRegistration* registration) {}
    virtual void OnRegistrationFinishedUninstalling(
        ServiceWorkerRegistration* registration) {}
    virtual void OnUpdateFound(
        ServiceWorkerRegistration* registration) {}
    virtual void OnSkippedWaiting(ServiceWorkerRegistration* registation) {}
  };

  ServiceWorkerRegistration(const GURL& pattern,
                            int64 registration_id,
                            base::WeakPtr<ServiceWorkerContextCore> context);

  int64 id() const { return registration_id_; }
  const GURL& pattern() const { return pattern_; }

  bool is_deleted() const { return is_deleted_; }
  void set_is_deleted(bool deleted) { is_deleted_ = deleted; }

  bool is_uninstalling() const { return is_uninstalling_; }
  bool is_uninstalled() const { return is_uninstalled_; }

  int64_t resources_total_size_bytes() const {
    return resources_total_size_bytes_;
  }

  void set_resources_total_size_bytes(int64_t resources_total_size_bytes) {
    resources_total_size_bytes_ = resources_total_size_bytes;
  }

  ServiceWorkerVersion* active_version() const {
    return active_version_.get();
  }

  ServiceWorkerVersion* waiting_version() const {
    return waiting_version_.get();
  }

  ServiceWorkerVersion* installing_version() const {
    return installing_version_.get();
  }

  ServiceWorkerVersion* GetNewestVersion() const;

  void AddListener(Listener* listener);
  void RemoveListener(Listener* listener);
  void NotifyRegistrationFailed();
  void NotifyUpdateFound();

  ServiceWorkerRegistrationInfo GetInfo();

  // Sets the corresposding version attribute and resets the position
  // (if any) left vacant (ie. by a waiting version being promoted).
  // Also notifies listeners via OnVersionAttributesChanged.
  void SetActiveVersion(ServiceWorkerVersion* version);
  void SetWaitingVersion(ServiceWorkerVersion* version);
  void SetInstallingVersion(ServiceWorkerVersion* version);

  // If version is the installing, waiting, active version of this
  // registation, the method will reset that field to NULL, and notify
  // listeners via OnVersionAttributesChanged.
  void UnsetVersion(ServiceWorkerVersion* version);

  // Triggers the [[Activate]] algorithm when the currently active version
  // has no controllees. If there are no controllees at the time the method
  // is called or when version's skip waiting flag is set, activation is
  // initiated immediately.
  void ActivateWaitingVersionWhenReady();

  // Takes over control of provider hosts which are currently not controlled or
  // controlled by other registrations.
  void ClaimClients(const StatusCallback& callback);

  // Triggers the [[ClearRegistration]] algorithm when the currently
  // active version has no controllees. Deletes this registration
  // from storage immediately.
  void ClearWhenReady();

  // Restores this registration in storage and cancels the pending
  // [[ClearRegistration]] algorithm.
  void AbortPendingClear(const StatusCallback& callback);

  // The time of the most recent update check.
  base::Time last_update_check() const { return last_update_check_; }
  void set_last_update_check(base::Time last) { last_update_check_ = last; }

  // Provide a storage mechanism to read/write arbitrary data associated with
  // this registration in the storage. Stored data is deleted when this
  // registration is deleted from the storage.
  void GetUserData(const std::string& key,
                   const GetUserDataCallback& callback);
  void StoreUserData(const std::string& key,
                     const std::string& data,
                     const StatusCallback& callback);
  void ClearUserData(const std::string& key,
                     const StatusCallback& callback);

 private:
  friend class base::RefCounted<ServiceWorkerRegistration>;

  ~ServiceWorkerRegistration() override;

  void SetVersionInternal(
      ServiceWorkerVersion* version,
      scoped_refptr<ServiceWorkerVersion>* data_member,
      int change_flag);
  void UnsetVersionInternal(
      ServiceWorkerVersion* version,
      ChangedVersionAttributesMask* mask);

  // ServiceWorkerVersion::Listener override.
  void OnNoControllees(ServiceWorkerVersion* version) override;

  // This method corresponds to the [[Activate]] algorithm.
  void ActivateWaitingVersion();
  void OnActivateEventFinished(
      ServiceWorkerVersion* activating_version,
      ServiceWorkerStatusCode status);
  void OnDeleteFinished(ServiceWorkerStatusCode status);

  // This method corresponds to the [[ClearRegistration]] algorithm.
  void Clear();

  void OnRestoreFinished(const StatusCallback& callback,
                         scoped_refptr<ServiceWorkerVersion> version,
                         ServiceWorkerStatusCode status);

  void DidGetRegistrationsForClaimClients(
      const StatusCallback& callback,
      scoped_refptr<ServiceWorkerVersion> version,
      const std::vector<ServiceWorkerRegistrationInfo>& registrations);
  bool ShouldClaim(
      ServiceWorkerProviderHost* provider_host,
      const std::vector<ServiceWorkerRegistrationInfo>& registration_infos);

  const GURL pattern_;
  const int64 registration_id_;
  bool is_deleted_;
  bool is_uninstalling_;
  bool is_uninstalled_;
  bool should_activate_when_ready_;
  base::Time last_update_check_;
  int64_t resources_total_size_bytes_;
  scoped_refptr<ServiceWorkerVersion> active_version_;
  scoped_refptr<ServiceWorkerVersion> waiting_version_;
  scoped_refptr<ServiceWorkerVersion> installing_version_;
  ObserverList<Listener> listeners_;
  base::WeakPtr<ServiceWorkerContextCore> context_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegistration);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_
