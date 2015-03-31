// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_MANAGER_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_MANAGER_H_

#include <list>
#include <map>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/cache_storage/cache_storage_scheduler.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerContextWrapper;

// BackgroundSyncManager manages and stores the set of background sync
// registrations across all registered service workers for a profile.
// Registrations are stored along with their associated Service Worker
// registration in ServiceWorkerStorage. If the ServiceWorker is unregistered,
// the sync registrations are removed. This class expects to be run on the IO
// thread. The asynchronous methods are executed sequentially.

// TODO(jkarlin): Check permissions when registering, scheduling, and firing
// background sync. In the meantime, --enable-service-worker-sync is required to
// fire a sync event.
// TODO(jkarlin): Unregister syncs when permission is revoked.
// TODO(jkarlin): Create a background sync scheduler to actually run the
// registered events.
// TODO(jkarlin): Keep the browser alive if "Let Google Chrome Run in the
// Background" is true and a sync is registered.
// TODO(jkarlin): Unregister syncs when storage for an origin is cleared.
// TODO(jkarlin): Detect and handle a corrupt or broken backend.
class CONTENT_EXPORT BackgroundSyncManager {
 public:
  enum ErrorType {
    ERROR_TYPE_OK = 0,
    ERROR_TYPE_STORAGE,
    ERROR_TYPE_NOT_FOUND
  };

  // TODO(jkarlin): Remove this and use the struct from IPC messages once it
  // lands.
  struct CONTENT_EXPORT BackgroundSyncRegistration {
    using RegistrationId = int64;
    static const RegistrationId kInvalidRegistrationId;

    BackgroundSyncRegistration()
        : BackgroundSyncRegistration(kInvalidRegistrationId, "") {}
    explicit BackgroundSyncRegistration(const std::string& name)
        : BackgroundSyncRegistration(kInvalidRegistrationId, name) {}
    BackgroundSyncRegistration(int64 id, const std::string& name)
        : id(id), min_period(0), name(name) {}

    bool Equals(const BackgroundSyncRegistration& other) {
      return this->name == other.name && this->min_period == other.min_period;
    }

    RegistrationId id;
    int64 min_period;
    std::string name;
  };

  struct CONTENT_EXPORT BackgroundSyncRegistrations {
    using NameToRegistrationMap =
        std::map<std::string, BackgroundSyncRegistration>;
    static const BackgroundSyncRegistration::RegistrationId kInitialId;

    BackgroundSyncRegistrations();
    explicit BackgroundSyncRegistrations(
        BackgroundSyncRegistration::RegistrationId next_id);
    ~BackgroundSyncRegistrations();

    NameToRegistrationMap name_to_registration_map;
    BackgroundSyncRegistration::RegistrationId next_id;
  };

  using StatusCallback = base::Callback<void(ErrorType)>;
  using StatusAndRegistrationCallback =
      base::Callback<void(ErrorType, const BackgroundSyncRegistration&)>;

  static scoped_ptr<BackgroundSyncManager> Create(
      const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context);
  virtual ~BackgroundSyncManager();

  // Stores the given background sync registration and adds it to the scheduling
  // queue. Overwrites any existing registration with the same name but
  // different parameters (other than the id). Calls |callback| with ErrorTypeOK
  // and the accepted registration on success. The accepted registration will
  // have a unique id. It may also have altered parameters if the user or UA
  // chose different parameters than those supplied.
  void Register(const GURL& origin,
                int64 sw_registration_id,
                const BackgroundSyncRegistration& sync_registration,
                const StatusAndRegistrationCallback& callback);

  // Removes the background sync registration with |sync_registration_name| if
  // the |sync_registration_id| matches. |sync_registration_id| will not match
  // if, for instance, a new registration with the same name has replaced it.
  // Calls |callback| with ErrorTypeNotFound if no match is found. Calls
  // |callback| with ErrorTypeOK on success.
  void Unregister(
      const GURL& origin,
      int64 sw_registration_id,
      const std::string& sync_registration_name,
      BackgroundSyncRegistration::RegistrationId sync_registration_id,
      const StatusCallback& callback);

  // Finds the background sync registration associated with
  // |sw_registration_id|. Calls |callback| with ErrorTypeNotFound if it doesn't
  // exist. Calls |callback| with ErrorTypeOK on success.
  void GetRegistration(const GURL& origin,
                       int64 sw_registration_id,
                       const std::string sync_registration_name,
                       const StatusAndRegistrationCallback& callback);

 protected:
  explicit BackgroundSyncManager(
      const scoped_refptr<ServiceWorkerContextWrapper>& context);

  // Init must be called before any public member function. Only call it once.
  void Init();

  // The following methods are virtual for testing.
  virtual void StoreDataInBackend(
      int64 sw_registration_id,
      const GURL& origin,
      const std::string& key,
      const std::string& data,
      const ServiceWorkerStorage::StatusCallback& callback);
  virtual void GetDataFromBackend(
      const std::string& key,
      const ServiceWorkerStorage::GetUserDataForAllRegistrationsCallback&
          callback);

 private:
  using PermissionStatusCallback = base::Callback<void(bool)>;
  using SWIdToRegistrationsMap = std::map<int64, BackgroundSyncRegistrations>;

  // Returns the existing registration in |existing_registration| if it is not
  // null.
  bool LookupRegistration(int64 sw_registration_id,
                          const std::string& sync_registration_name,
                          BackgroundSyncRegistration* existing_registration);

  // Store all registrations for a given |sw_registration_id|.
  void StoreRegistrations(const GURL& origin,
                          int64 sw_registration_id,
                          const ServiceWorkerStorage::StatusCallback& callback);

  // If the registration is in the map, removes it and returns the removed
  // registration in |old_registration|. |old_registration| may be null.
  void RemoveRegistrationFromMap(int64 sw_registration_id,
                                 const std::string& sync_registration_name,
                                 BackgroundSyncRegistration* old_registration);

  void AddRegistrationToMap(
      int64 sw_registration_id,
      const BackgroundSyncRegistration& sync_registration);

  void InitImpl();
  void InitDidGetDataFromBackend(
      const std::vector<std::pair<int64, std::string>>& user_data,
      ServiceWorkerStatusCode status);

  // Register callbacks
  void RegisterImpl(const GURL& origin,
                    int64 sw_registration_id,
                    const BackgroundSyncRegistration& sync_registration,
                    const StatusAndRegistrationCallback& callback);
  void RegisterDidStore(int64 sw_registration_id,
                        const BackgroundSyncRegistration& sync_registration,
                        const BackgroundSyncRegistration& previous_registration,
                        const StatusAndRegistrationCallback& callback,
                        ServiceWorkerStatusCode status);

  // Unregister callbacks
  void UnregisterImpl(
      const GURL& origin,
      int64 sw_registration_id,
      const std::string& sync_registration_name,
      BackgroundSyncRegistration::RegistrationId sync_registration_id,
      const StatusCallback& callback);
  void UnregisterDidStore(
      int64 sw_registration_id,
      const BackgroundSyncRegistration& old_sync_registration,
      const StatusCallback& callback,
      ServiceWorkerStatusCode status);

  // GetRegistration callbacks
  void GetRegistrationImpl(const GURL& origin,
                           int64 sw_registration_id,
                           const std::string sync_registration_name,
                           const StatusAndRegistrationCallback& callback);

  // Operation Scheduling callbacks
  void PendingStatusAndRegistrationCallback(
      const StatusAndRegistrationCallback& callback,
      ErrorType error,
      const BackgroundSyncRegistration& sync_registration);
  void PendingStatusCallback(const StatusCallback& callback, ErrorType error);

  SWIdToRegistrationsMap sw_to_registrations_map_;
  CacheStorageScheduler op_scheduler_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  base::WeakPtrFactory<BackgroundSyncManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_MANAGER_H_
