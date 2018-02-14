// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_CRYPTAUTH_DEVICE_MANAGER_IMPL_H_
#define COMPONENTS_CRYPTAUTH_CRYPTAUTH_DEVICE_MANAGER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/cryptauth/cryptauth_device_manager.h"
#include "components/cryptauth/cryptauth_gcm_manager.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/sync_scheduler.h"

class PrefService;

namespace cryptauth {

class CryptAuthClient;
class CryptAuthClientFactory;

class CryptAuthDeviceManagerImpl : public CryptAuthDeviceManager,
                                   public SyncScheduler::Delegate,
                                   public CryptAuthGCMManager::Observer {
 public:
  // Creates the manager:
  // |clock|: Used to determine the time between sync attempts.
  // |client_factory|: Creates CryptAuthClient instances to perform each sync.
  // |gcm_manager|: Notifies when GCM push messages trigger device syncs.
  //                Not owned and must outlive this instance.
  // |pref_service|: Stores syncing metadata and unlock key information to
  //                 persist across browser restarts. Must already be registered
  //                 with RegisterPrefs().
  CryptAuthDeviceManagerImpl(
      base::Clock* clock,
      std::unique_ptr<CryptAuthClientFactory> client_factory,
      CryptAuthGCMManager* gcm_manager,
      PrefService* pref_service);
  ~CryptAuthDeviceManagerImpl() override;

  // CryptAuthDeviceManager:
  void Start() override;
  void ForceSyncNow(InvocationReason invocation_reason) override;
  base::Time GetLastSyncTime() const override;
  base::TimeDelta GetTimeToNextAttempt() const override;
  bool IsSyncInProgress() const override;
  bool IsRecoveringFromFailure() const override;
  std::vector<ExternalDeviceInfo> GetSyncedDevices() const override;
  std::vector<ExternalDeviceInfo> GetUnlockKeys() const override;
  std::vector<ExternalDeviceInfo> GetPixelUnlockKeys() const override;
  std::vector<ExternalDeviceInfo> GetTetherHosts() const override;
  std::vector<ExternalDeviceInfo> GetPixelTetherHosts() const override;

 protected:
  void SetSyncSchedulerForTest(std::unique_ptr<SyncScheduler> sync_scheduler);

 private:
  // CryptAuthGCMManager::Observer:
  void OnResyncMessage() override;

  // Updates |unlock_keys_| by fetching the list stored in |pref_service_|.
  void UpdateUnlockKeysFromPrefs();

  // SyncScheduler::Delegate:
  void OnSyncRequested(
      std::unique_ptr<SyncScheduler::SyncRequest> sync_request) override;

  // Callback when |cryptauth_client_| completes with the response.
  void OnGetMyDevicesSuccess(const GetMyDevicesResponse& response);
  void OnGetMyDevicesFailure(const std::string& error);

  // Used to determine the time.
  base::Clock* clock_;

  // Creates CryptAuthClient instances for each sync attempt.
  std::unique_ptr<CryptAuthClientFactory> client_factory_;

  // Notifies when GCM push messages trigger device sync. Not owned and must
  // outlive this instance.
  CryptAuthGCMManager* gcm_manager_;

  // Contains preferences that outlive the lifetime of this object and across
  // process restarts. |pref_service_| must outlive the lifetime of this
  // instance.
  PrefService* const pref_service_;

  // All devices currently synced from CryptAuth.
  std::vector<ExternalDeviceInfo> synced_devices_;

  // Schedules the time between device sync attempts.
  std::unique_ptr<SyncScheduler> scheduler_;

  // Contains the SyncRequest that |scheduler_| requests when a device sync
  // attempt is made.
  std::unique_ptr<SyncScheduler::SyncRequest> sync_request_;

  // The CryptAuthEnroller instance for the current sync attempt. A new
  // instance will be created for each individual attempt.
  std::unique_ptr<CryptAuthClient> cryptauth_client_;

  base::WeakPtrFactory<CryptAuthDeviceManagerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthDeviceManagerImpl);
};

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_CRYPTAUTH_DEVICE_MANAGER_IMPL_H_
