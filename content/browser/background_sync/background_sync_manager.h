// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_MANAGER_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "content/browser/background_sync/background_sync.pb.h"
#include "content/browser/background_sync/background_sync_registration.h"
#include "content/browser/background_sync/background_sync_registration_handle.h"
#include "content/browser/background_sync/background_sync_status.h"
#include "content/browser/cache_storage/cache_storage_scheduler.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/background_sync_service.mojom.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/browser/background_sync_parameters.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace content {

class BackgroundSyncNetworkObserver;
class ServiceWorkerContextWrapper;

// BackgroundSyncManager manages and stores the set of background sync
// registrations across all registered service workers for a profile.
// Registrations are stored along with their associated Service Worker
// registration in ServiceWorkerStorage. If the ServiceWorker is unregistered,
// the sync registrations are removed. This class must be run on the IO
// thread. The asynchronous methods are executed sequentially.
class CONTENT_EXPORT BackgroundSyncManager
    : NON_EXPORTED_BASE(public ServiceWorkerContextObserver) {
 public:
  using BoolCallback = base::Callback<void(bool)>;
  using StatusCallback = base::Callback<void(BackgroundSyncStatus)>;
  using StatusAndRegistrationCallback =
      base::Callback<void(BackgroundSyncStatus,
                          scoped_ptr<BackgroundSyncRegistrationHandle>)>;
  using StatusAndStateCallback =
      base::Callback<void(BackgroundSyncStatus, BackgroundSyncState)>;
  using StatusAndRegistrationsCallback = base::Callback<void(
      BackgroundSyncStatus,
      scoped_ptr<ScopedVector<BackgroundSyncRegistrationHandle>>)>;

  static scoped_ptr<BackgroundSyncManager> Create(
      const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context);
  ~BackgroundSyncManager() override;

  // Stores the given background sync registration and adds it to the scheduling
  // queue. It will overwrite an existing registration with the same tag unless
  // they're identical (save for the id). Calls |callback| with
  // BACKGROUND_SYNC_STATUS_OK and the accepted registration on success.
  // The accepted registration will have a unique id. It may also have altered
  // parameters if the user or UA chose different parameters than those
  // supplied.
  void Register(int64_t sw_registration_id,
                const BackgroundSyncRegistrationOptions& options,
                bool requested_from_service_worker,
                const StatusAndRegistrationCallback& callback);

  // Finds the background sync registration associated with
  // |sw_registration_id|, and tag |sync_registration_tag|. Calls |callback|
  // with BACKGROUND_SYNC_STATUS_NOT_FOUND if it doesn't exist. Calls |callback|
  // with BACKGROUND_SYNC_STATUS_OK on success. If the callback's status is not
  // BACKGROUND_SYNC_STATUS_OK then the callback's RegistrationHandle will be
  // nullptr.
  void GetRegistration(int64_t sw_registration_id,
                       const std::string& sync_registration_tag,
                       const StatusAndRegistrationCallback& callback);

  // Finds the background sync registrations associated with
  // |sw_registration_id|. Calls |callback| with BACKGROUND_SYNC_STATUS_OK on
  // success.
  void GetRegistrations(int64_t sw_registration_id,
                        const StatusAndRegistrationsCallback& callback);

  // Given a HandleId |handle_id|, return a new handle for the same
  // registration.
  scoped_ptr<BackgroundSyncRegistrationHandle> DuplicateRegistrationHandle(
      BackgroundSyncRegistrationHandle::HandleId handle_id);

  // ServiceWorkerContextObserver overrides.
  void OnRegistrationDeleted(int64_t sw_registration_id,
                             const GURL& pattern) override;
  void OnStorageWiped() override;

  // Sets the max number of sync attempts after any pending operations have
  // completed.
  void SetMaxSyncAttemptsForTesting(int max_attempts);

  BackgroundSyncNetworkObserver* GetNetworkObserverForTesting() {
    return network_observer_.get();
  }

  void set_clock(scoped_ptr<base::Clock> clock) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    clock_ = std::move(clock);
  }

 protected:
  // A registration might be referenced by the client longer than
  // the BackgroundSyncManager needs to keep track of it (e.g., the event has
  // finished firing). The BackgroundSyncManager reference counts its
  // registrations internally and every BackgroundSyncRegistrationHandle has a
  // unique handle id which maps to a locally maintained (in
  // client_registration_ids_) scoped_refptr.
  class RefCountedRegistration;

  explicit BackgroundSyncManager(
      const scoped_refptr<ServiceWorkerContextWrapper>& context);

  // Init must be called before any public member function. Only call it once.
  void Init();

  // The following methods are virtual for testing.
  virtual void StoreDataInBackend(
      int64_t sw_registration_id,
      const GURL& origin,
      const std::string& backend_key,
      const std::string& data,
      const ServiceWorkerStorage::StatusCallback& callback);
  virtual void GetDataFromBackend(
      const std::string& backend_key,
      const ServiceWorkerStorage::GetUserDataForAllRegistrationsCallback&
          callback);
  virtual void FireOneShotSync(
      BackgroundSyncRegistrationHandle::HandleId handle_id,
      const scoped_refptr<ServiceWorkerVersion>& active_version,
      BackgroundSyncEventLastChance last_chance,
      const ServiceWorkerVersion::StatusCallback& callback);
  virtual void ScheduleDelayedTask(const base::Closure& callback,
                                   base::TimeDelta delay);
  virtual void HasMainFrameProviderHost(const GURL& origin,
                                        const BoolCallback& callback);

 private:
  friend class TestBackgroundSyncManager;
  friend class BackgroundSyncManagerTest;
  friend class BackgroundSyncRegistrationHandle;

  class RegistrationKey {
   public:
    explicit RegistrationKey(const BackgroundSyncRegistration& registration);
    explicit RegistrationKey(const BackgroundSyncRegistrationOptions& options);
    RegistrationKey(const std::string& tag);
    RegistrationKey(const RegistrationKey& other) = default;
    RegistrationKey& operator=(const RegistrationKey& other) = default;

    bool operator<(const RegistrationKey& rhs) const {
      return value_ < rhs.value_;
    }

   private:
    std::string value_;
  };

  struct BackgroundSyncRegistrations {
    using RegistrationMap =
        std::map<RegistrationKey, scoped_refptr<RefCountedRegistration>>;

    BackgroundSyncRegistrations();
    ~BackgroundSyncRegistrations();

    RegistrationMap registration_map;
    BackgroundSyncRegistration::RegistrationId next_id;
    GURL origin;
  };

  using PermissionStatusCallback = base::Callback<void(bool)>;
  using SWIdToRegistrationsMap = std::map<int64_t, BackgroundSyncRegistrations>;

  static const size_t kMaxTagLength = 10240;

  scoped_ptr<BackgroundSyncRegistrationHandle> CreateRegistrationHandle(
      const scoped_refptr<RefCountedRegistration>& registration);

  // Returns the BackgroundSyncRegistration corresponding to |handle_id|.
  // Returns nullptr if the registration is not found.
  BackgroundSyncRegistration* GetRegistrationForHandle(
      BackgroundSyncRegistrationHandle::HandleId handle_id) const;

  // The BackgroundSyncManager holds references to registrations that have
  // active Handles. The handles must call this on destruction.
  void ReleaseRegistrationHandle(
      BackgroundSyncRegistrationHandle::HandleId handle_id);

  // Disable the manager. Already queued operations will abort once they start
  // to run (in their impl methods). Future operations will not queue. The one
  // exception is already firing events -- their responses will be processed in
  // order to notify their final state.
  // The list of active registrations is cleared and the backend is also cleared
  // (if it's still functioning). The manager will reenable itself once it
  // receives the OnStorageWiped message or on browser restart.
  void DisableAndClearManager(const base::Closure& callback);
  void DisableAndClearDidGetRegistrations(
      const base::Closure& callback,
      const std::vector<std::pair<int64_t, std::string>>& user_data,
      ServiceWorkerStatusCode status);
  void DisableAndClearManagerClearedOne(const base::Closure& barrier_closure,
                                        ServiceWorkerStatusCode status);

  // Returns the existing registration or nullptr if it cannot be found.
  RefCountedRegistration* LookupActiveRegistration(
      int64_t sw_registration_id,
      const RegistrationKey& registration_key);

  // Write all registrations for a given |sw_registration_id| to persistent
  // storage.
  void StoreRegistrations(int64_t sw_registration_id,
                          const ServiceWorkerStorage::StatusCallback& callback);

  // Removes the active registration if it is in the map.
  void RemoveActiveRegistration(int64_t sw_registration_id,
                                const RegistrationKey& registration_key);

  void AddActiveRegistration(
      int64_t sw_registration_id,
      const GURL& origin,
      const scoped_refptr<RefCountedRegistration>& sync_registration);

  void InitImpl(const base::Closure& callback);
  void InitDidGetControllerParameters(
      const base::Closure& callback,
      scoped_ptr<BackgroundSyncParameters> parameters);
  void InitDidGetDataFromBackend(
      const base::Closure& callback,
      const std::vector<std::pair<int64_t, std::string>>& user_data,
      ServiceWorkerStatusCode status);

  // Register callbacks
  void RegisterCheckIfHasMainFrame(
      int64_t sw_registration_id,
      const BackgroundSyncRegistrationOptions& options,
      const StatusAndRegistrationCallback& callback);
  void RegisterDidCheckIfMainFrame(
      int64_t sw_registration_id,
      const BackgroundSyncRegistrationOptions& options,
      const StatusAndRegistrationCallback& callback,
      bool has_main_frame_client);
  void RegisterImpl(int64_t sw_registration_id,
                    const BackgroundSyncRegistrationOptions& options,
                    const StatusAndRegistrationCallback& callback);
  void RegisterDidStore(
      int64_t sw_registration_id,
      const scoped_refptr<RefCountedRegistration>& new_registration_ref,
      const StatusAndRegistrationCallback& callback,
      ServiceWorkerStatusCode status);

  // Removes the background sync with id |sync_registration_id|. Calls
  // |callback| with BACKGROUND_SYNC_STATUS_NOT_FOUND if no match is found.
  // Calls |callback| with BACKGROUND_SYNC_STATUS_OK on success.
  void Unregister(int64_t sw_registration_id,
                  BackgroundSyncRegistrationHandle::HandleId handle_id,
                  const StatusCallback& callback);
  void UnregisterImpl(
      int64_t sw_registration_id,
      const RegistrationKey& key,
      BackgroundSyncRegistration::RegistrationId sync_registration_id,
      const StatusCallback& callback);
  void UnregisterDidStore(int64_t sw_registration_id,
                          const StatusCallback& callback,
                          ServiceWorkerStatusCode status);

  // NotifyWhenFinished and its callbacks. See
  // BackgroundSyncRegistrationHandle::NotifyWhenFinished for detailed
  // documentation.
  void NotifyWhenFinished(BackgroundSyncRegistrationHandle::HandleId handle_id,
                          const StatusAndStateCallback& callback);
  void NotifyWhenFinishedImpl(
      scoped_ptr<BackgroundSyncRegistrationHandle> registration_handle,
      const StatusAndStateCallback& callback);
  void NotifyWhenFinishedInvokeCallback(const StatusAndStateCallback& callback,
                                        BackgroundSyncState status);

  // GetRegistration callbacks
  void GetRegistrationImpl(int64_t sw_registration_id,
                           const RegistrationKey& registration_key,
                           const StatusAndRegistrationCallback& callback);

  // GetRegistrations callbacks
  void GetRegistrationsImpl(int64_t sw_registration_id,
                            const StatusAndRegistrationsCallback& callback);

  bool AreOptionConditionsMet(const BackgroundSyncRegistrationOptions& options);
  bool IsRegistrationReadyToFire(
      const BackgroundSyncRegistration& registration);

  // Determines if the browser needs to be able to run in the background (e.g.,
  // to run a pending registration or verify that a firing registration
  // completed). If background processing is required it calls out to the
  // BackgroundSyncController to enable it.
  // Assumes that all registrations in the pending state are not currently ready
  // to fire. Therefore this should not be called directly and should only be
  // called by FireReadyEvents.
  void RunInBackgroundIfNecessary();

  // FireReadyEvents scans the list of available events and fires those that are
  // ready to fire. For those that can't yet be fired, wakeup alarms are set.
  void FireReadyEvents();
  void FireReadyEventsImpl(const base::Closure& callback);
  void FireReadyEventsDidFindRegistration(
      const RegistrationKey& registration_key,
      BackgroundSyncRegistration::RegistrationId registration_id,
      const base::Closure& event_fired_callback,
      const base::Closure& event_completed_callback,
      ServiceWorkerStatusCode service_worker_status,
      const scoped_refptr<ServiceWorkerRegistration>&
          service_worker_registration);
  void FireReadyEventsAllEventsFiring(const base::Closure& callback);

  // Called when a sync event has completed.
  void EventComplete(
      const scoped_refptr<ServiceWorkerRegistration>&
          service_worker_registration,
      int64_t service_worker_id,
      scoped_ptr<BackgroundSyncRegistrationHandle> registration_handle,
      const base::Closure& callback,
      ServiceWorkerStatusCode status_code);
  void EventCompleteImpl(
      int64_t service_worker_id,
      scoped_ptr<BackgroundSyncRegistrationHandle> registration_handle,
      ServiceWorkerStatusCode status_code,
      const base::Closure& callback);
  void EventCompleteDidStore(int64_t service_worker_id,
                             const base::Closure& callback,
                             ServiceWorkerStatusCode status_code);

  // Called when all sync events have completed.
  static void OnAllSyncEventsCompleted(const base::TimeTicks& start_time,
                                       int number_of_batched_sync_events);

  // OnRegistrationDeleted callbacks
  void OnRegistrationDeletedImpl(int64_t sw_registration_id,
                                 const base::Closure& callback);

  // OnStorageWiped callbacks
  void OnStorageWipedImpl(const base::Closure& callback);

  void OnNetworkChanged();

  // SetMaxSyncAttempts callback
  void SetMaxSyncAttemptsImpl(int max_sync_attempts,
                              const base::Closure& callback);

  // Operation Scheduling callback and convenience functions.
  template <typename CallbackT, typename... Params>
  void CompleteOperationCallback(const CallbackT& callback,
                                 Params... parameters);
  void CompleteStatusAndRegistrationCallback(
      StatusAndRegistrationCallback callback,
      BackgroundSyncStatus status,
      scoped_ptr<BackgroundSyncRegistrationHandle> result);
  void CompleteStatusAndRegistrationsCallback(
      StatusAndRegistrationsCallback callback,
      BackgroundSyncStatus status,
      scoped_ptr<ScopedVector<BackgroundSyncRegistrationHandle>> results);
  base::Closure MakeEmptyCompletion();
  base::Closure MakeClosureCompletion(const base::Closure& callback);
  StatusAndRegistrationCallback MakeStatusAndRegistrationCompletion(
      const StatusAndRegistrationCallback& callback);
  StatusAndRegistrationsCallback MakeStatusAndRegistrationsCompletion(
      const StatusAndRegistrationsCallback& callback);
  BackgroundSyncManager::StatusCallback MakeStatusCompletion(
      const StatusCallback& callback);

  SWIdToRegistrationsMap active_registrations_;
  CacheStorageScheduler op_scheduler_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  scoped_ptr<BackgroundSyncParameters> parameters_;

  // True if the manager is disabled and registrations should fail.
  bool disabled_;

  // The number of registrations currently in the firing state.
  int num_firing_registrations_;

  base::CancelableCallback<void()> delayed_sync_task_;

  scoped_ptr<BackgroundSyncNetworkObserver> network_observer_;

  // The registrations that clients have handles to.
  IDMap<scoped_refptr<RefCountedRegistration>,
        IDMapOwnPointer,
        BackgroundSyncRegistrationHandle::HandleId> registration_handle_ids_;

  scoped_ptr<base::Clock> clock_;

  base::WeakPtrFactory<BackgroundSyncManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_MANAGER_H_
