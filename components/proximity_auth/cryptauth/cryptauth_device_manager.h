// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_CRYPTAUTH_DEVICE_MANAGER_H
#define COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_CRYPTAUTH_DEVICE_MANAGER_H

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/proximity_auth/cryptauth/cryptauth_gcm_manager.h"
#include "components/proximity_auth/cryptauth/proto/cryptauth_api.pb.h"
#include "components/proximity_auth/cryptauth/sync_scheduler.h"

class PrefService;
class PrefRegistrySimple;

namespace proximity_auth {

class CryptAuthClient;
class CryptAuthClientFactory;

// This class manages syncing and storing the user's phones that are registered
// with CryptAuth and are capable of unlocking the user's other devices. These
// phones are called "unlock keys".
// The manager periodically syncs the user's devices from CryptAuth to keep the
// list of unlock keys fresh. If a sync attempts fails, the manager will
// schedule the next sync more aggressively to recover.
class CryptAuthDeviceManager : public SyncScheduler::Delegate,
                               public CryptAuthGCMManager::Observer {
 public:
  // Respresents the success result of a sync attempt.
  enum class SyncResult { SUCCESS, FAILURE };

  // Represents whether the list of unlock keys has changed after a sync
  // attempt completes.
  enum class DeviceChangeResult {
    UNCHANGED,
    CHANGED,
  };

  class Observer {
   public:
    // Called when a sync attempt is started.
    virtual void OnSyncStarted() {}

    // Called when a sync attempt finishes with the |success| of the request.
    // |devices_changed| specifies if the sync caused the stored unlock keys to
    // change.
    virtual void OnSyncFinished(SyncResult sync_result,
                                DeviceChangeResult device_change_result) {}

    virtual ~Observer() {}
  };

  // Creates the manager:
  // |clock|: Used to determine the time between sync attempts.
  // |client_factory|: Creates CryptAuthClient instances to perform each sync.
  // |gcm_manager|: Notifies when GCM push messages trigger device syncs.
  //                Not owned and must outlive this instance.
  // |pref_service|: Stores syncing metadata and unlock key information to
  //                 persist across browser restarts. Must already be registered
  //                 with RegisterPrefs().
  CryptAuthDeviceManager(std::unique_ptr<base::Clock> clock,
                         std::unique_ptr<CryptAuthClientFactory> client_factory,
                         CryptAuthGCMManager* gcm_manager,
                         PrefService* pref_service);

  ~CryptAuthDeviceManager() override;

  // Registers the prefs used by this class to the given |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Starts device manager to begin syncing devices.
  void Start();

  // Adds an observer.
  void AddObserver(Observer* observer);

  // Removes an observer.
  void RemoveObserver(Observer* observer);

  // Skips the waiting period and forces a sync immediately. If a
  // sync attempt is already in progress, this function does nothing.
  // |invocation_reason| specifies the reason that the sync was triggered,
  // which is upload to the server.
  void ForceSyncNow(cryptauth::InvocationReason invocation_reason);

  // Returns the timestamp of the last successful sync. If no sync
  // has ever been made, then returns a null base::Time object.
  base::Time GetLastSyncTime() const;

  // Returns the time to the next sync attempt.
  base::TimeDelta GetTimeToNextAttempt() const;

  // Returns true if a device sync attempt is currently in progress.
  bool IsSyncInProgress() const;

  // Returns true if the last device sync failed and the manager is now
  // scheduling sync attempts more aggressively to recover. If no enrollment
  // has ever been recorded, then this function will also return true.
  bool IsRecoveringFromFailure() const;

  // Returns a list of remote devices that can unlock the user's other devices.
  const std::vector<cryptauth::ExternalDeviceInfo>& unlock_keys() const {
    return unlock_keys_;
  }

 protected:
  // Creates a new SyncScheduler instance. Exposed for testing.
  virtual std::unique_ptr<SyncScheduler> CreateSyncScheduler();

 private:
  // CryptAuthGCMManager::Observer:
  void OnResyncMessage() override;

  // Updates |unlock_keys_| by fetching the list stored in |pref_service_|.
  void UpdateUnlockKeysFromPrefs();

  // SyncScheduler::Delegate:
  void OnSyncRequested(
      std::unique_ptr<SyncScheduler::SyncRequest> sync_request) override;

  // Callback when |cryptauth_client_| completes with the response.
  void OnGetMyDevicesSuccess(const cryptauth::GetMyDevicesResponse& response);
  void OnGetMyDevicesFailure(const std::string& error);

  // Used to determine the time.
  std::unique_ptr<base::Clock> clock_;

  // Creates CryptAuthClient instances for each sync attempt.
  std::unique_ptr<CryptAuthClientFactory> client_factory_;

  // Notifies when GCM push messages trigger device sync. Not owned and must
  // outlive this instance.
  CryptAuthGCMManager* gcm_manager_;

  // Contains preferences that outlive the lifetime of this object and across
  // process restarts. |pref_service_| must outlive the lifetime of this
  // instance.
  PrefService* const pref_service_;

  // The unlock keys currently synced from CryptAuth.
  std::vector<cryptauth::ExternalDeviceInfo> unlock_keys_;

  // Schedules the time between device sync attempts.
  std::unique_ptr<SyncScheduler> scheduler_;

  // Contains the SyncRequest that |scheduler_| requests when a device sync
  // attempt is made.
  std::unique_ptr<SyncScheduler::SyncRequest> sync_request_;

  // The CryptAuthEnroller instance for the current sync attempt. A new
  // instance will be created for each individual attempt.
  std::unique_ptr<CryptAuthClient> cryptauth_client_;

  // List of observers.
  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<CryptAuthDeviceManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthDeviceManager);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_CRYPTAUTH_CRYPTAUTH_DEVICE_MANAGER_H
